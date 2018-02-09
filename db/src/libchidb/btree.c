/*
 *  chidb - a didactic relational database management system
 *
 * This module contains functions to manipulate a B-Tree file. In this context,
 * "BTree" refers not to a single B-Tree but to a "file of B-Trees" ("chidb
 * file" and "file of B-Trees" are essentially equivalent terms).
 *
 * However, this module does *not* read or write to the database file directly.
 * All read/write operations must be done through the pager module.
 *
 */

/*
 *  Copyright (c) 2009-2015, The University of Chicago
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or withsend
 *  modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  - Neither the name of The University of Chicago nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software withsend specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY send OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <chidb/log.h>
#include "chidbInt.h"
#include "btree.h"
#include "record.h"
#include "pager.h"
#include "util.h"

#define READ_VARINT32(var, buffer, offset) uint32_t var; getVarint32(buffer + offset, &var);

// initialize a btn's right page and celloffset array given its other initial values
void update_fields(BTreeNode *btn, int header_offset) {
    if (btn->type == PGTYPE_TABLE_INTERNAL || btn->type == PGTYPE_INDEX_INTERNAL) {
        btn->right_page = (npage_t)get4byte(btn->page->data + header_offset + PGHEADER_RIGHTPG_OFFSET);
        btn->celloffset_array = btn->page->data + header_offset + INTPG_CELLSOFFSET_OFFSET;
    } else { // leaf page
        btn->right_page = 0; // leaves have no right pointers
        btn->celloffset_array = btn->page->data + header_offset + LEAFPG_CELLSOFFSET_OFFSET;
    }
}

// Return a new empty BTreeNode
BTreeNode chidb_Btree_createNode(BTree *bt, npage_t npage, uint8_t type) {
    MemPage *page = malloc(sizeof(MemPage));
    *page = chidb_Pager_initMemPage(npage, bt->pager->page_size);
    int header_offset = npage == 1 ? 100 : 0;
    bool is_leaf = type == PGTYPE_TABLE_LEAF || type ==  PGTYPE_INDEX_LEAF;
    uint16_t free_offset = is_leaf ? LEAFPG_CELLSOFFSET_OFFSET : INTPG_CELLSOFFSET_OFFSET;
    BTreeNode new_node = {
        .page=page,
        .type=type,
        .free_offset=free_offset + header_offset,
        .n_cells=0,
        .cells_offset=bt->pager->page_size,
    };
    update_fields(&new_node, header_offset);
    return new_node;
}

/* Open a B-Tree file
 *
 * This function opens a database file and verifies that the file
 * header is correct. If the file is empty (which will happen
 * if the pager is given a filename for a file that does not exist)
 * then this function will (1) initialize the file header using
 * the default page size and (2) create an empty table leaf node
 * in page 1.
 *
 * Parameters
 * - filename: Database file (might not exist)
 * - db: A chidb struct. Its bt field must be set to the newly
 *       created BTree.
 * - bt: An out parameter. Used to return a pointer to the
 *       newly created BTree.
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ECORRUPTHEADER: Database file contains an invalid header
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_open(const char *filename, chidb *db, BTree **bt)
{
    FILE *f = fopen(filename, "r+");
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f) - 1;
    rewind(f);
    if (f && file_size >= 0) {
        // TODO: use functions from pager.c to manage Pager?
        uint8_t buffer[100];
        int num_read;
        if ((num_read = fread(buffer, 1, 100, f)) < 100) {
            chilog(WARNING, "nonempty database file is smaller than 100 bytes");
            return CHIDB_ECORRUPTHEADER;
        }

        char *expected = "SQLite format 3";
        if (strncmp((char *)buffer, expected, strlen(expected))) {
            chilog(WARNING, "database file is not SQLite format 3");
            return CHIDB_ECORRUPTHEADER;
        }

        uint16_t page_size = get2byte(buffer + 16);
        uint32_t file_change_counter = get4byte(buffer + 24);
        uint32_t schema_version = get4byte(buffer + 40);
        uint32_t page_cache_size = get4byte(buffer + 48);
        uint32_t user_cookie = get4byte(buffer + 60);

        if (file_change_counter || schema_version || user_cookie || page_cache_size != 20000) {
            return CHIDB_ECORRUPTHEADER;
        }

        Pager *pager = malloc(sizeof(Pager));
        pager->f = f;
        pager->page_size = page_size;
        pager->n_pages = (npage_t) file_size / page_size;

        *bt = malloc(sizeof(BTree));
        (*bt)->pager = pager;
        (*bt)->db = db;

        db->bt = *bt;
        // fclose(f);
    } else {
        Pager *pager = malloc(sizeof(Pager));
        pager->f = f;
        pager->page_size = DEFAULT_PAGE_SIZE;
        pager->n_pages = 1;

        *bt = malloc(sizeof(BTree));
        (*bt)->pager = pager;
        (*bt)->db = db;
        db->bt = *bt;

        // write empty leaf node into mem
        BTreeNode root = chidb_Btree_createNode(*bt, 1, PGTYPE_TABLE_LEAF);

        // write header into mem
        strcpy((char *)root.page->data, "SQLite format 3");
        put2byte(root.page->data + 16, DEFAULT_PAGE_SIZE);
        put4byte(root.page->data + 48, 20000);


        int result = chidb_Btree_writeNode(*bt, &root);
        return result;
    }
    return CHIDB_OK;
}


/* Close a B-Tree file
 *
 * This function closes a database file, freeing any resource
 * used in memory, such as the pager.
 *
 * Parameters
 * - bt: B-Tree file to close
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_close(BTree *bt)
{
    // chidb_close(bt->db);
    chidb_Pager_close(bt->pager);;
    free(bt);
    return CHIDB_OK;
}

/* Loads a B-Tree node from disk
 *
 * Reads a B-Tree node from a page in the disk. All the information regarding
 * the node is stored in a BTreeNode struct (see header file for more details
 * on this struct). *This is the only function that can allocate memory for
 * a BTreeNode struct*. Always use chidb_Btree_freeMemNode to free the memory
 * allocated for a BTreeNode (do not use free() directly on a BTreeNode variable)
 * Any changes made to a BTreeNode variable will not be effective in the database
 * until chidb_Btree_writeNode is called on that BTreeNode.
 *
 * Parameters
 * - bt: B-Tree file
 * - npage: Page of node to load
 * - btn: Out parameter. Used to return a pointer to newly creater BTreeNode
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EPAGENO: The provided page number is not valid
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_getNodeByPage(BTree *bt, npage_t npage, BTreeNode **btn)
{
    MemPage *page;
    int result;
    if ((result = chidb_Pager_readPage(bt->pager, npage, &page)) != CHIDB_OK) {
        return result;
    }

    *btn = malloc(sizeof(BTreeNode));
    // the first page's page header starts at byte 100 because of the file header
    int header_offset = npage == 1 ? 100 : 0;
    (*btn)->type = page->data[header_offset + PGHEADER_PGTYPE_OFFSET];
    (*btn)->free_offset = get2byte(page->data + header_offset + PGHEADER_FREE_OFFSET);
    (*btn)->n_cells = get2byte(page->data + header_offset + PGHEADER_NCELLS_OFFSET);
    (*btn)->cells_offset = get2byte(page->data + header_offset + PGHEADER_CELL_OFFSET);
    (*btn)->page = page;

    update_fields(*btn, header_offset);

    return CHIDB_OK;
}


/* Frees the memory allocated to an in-memory B-Tree node
 *
 * Frees the memory allocated to an in-memory B-Tree node, and
 * the in-memory page returned by the pages (stored in the
 * "page" field of BTreeNode)
 *
 * Parameters
 * - bt: B-Tree file
 * - btn: BTreeNode to free
 *
 * Return
 * - CHIDB_OK: Operation successful
 */
int chidb_Btree_freeMemNode(BTree *bt, BTreeNode *btn)
{
    int result;
    if ((result = chidb_Pager_releaseMemPage(bt->pager, btn->page)) != CHIDB_OK) {
        return result;
    }
    free(btn);

    return CHIDB_OK;
}



/* Syncs the values of a BTNode with its in-memory page
 *
 * Since the cell offset array and the cells themselves are modified directly on the
 * page, the values we need to update are "type", "free_offset", "n_cells",
 * "cells_offset" and "right_page".
 *
 * Parameters
 * - btn: the BTreeNode to sync
 */
void chidb_Btree_syncNode(BTreeNode *btn)
{
    int header_offset = btn->page->npage == 1 ? 100 : 0;
    btn->page->data[header_offset + PGHEADER_PGTYPE_OFFSET] = btn->type;
    put2byte(btn->page->data + header_offset + PGHEADER_FREE_OFFSET, btn->free_offset);
    put2byte(btn->page->data + header_offset + PGHEADER_NCELLS_OFFSET, btn->n_cells);
    put2byte(btn->page->data + header_offset + PGHEADER_CELL_OFFSET, btn->cells_offset);
    if (btn->type == PGTYPE_TABLE_INTERNAL || btn->type == PGTYPE_INDEX_INTERNAL) {
        put4byte(btn->page->data + header_offset + PGHEADER_RIGHTPG_OFFSET, btn->right_page);
    }
}


/* Write an in-memory B-Tree node to disk
 *
 * Parameters
 * - bt: B-Tree file
 * - btn: BTreeNode to write to disk
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_writeNode(BTree *bt, BTreeNode *btn)
{
    chidb_Btree_syncNode(btn);
    return chidb_Pager_writePage(bt->pager, btn->page);
}


/* Create a new B-Tree node
 *
 * Allocates a new page in the file and initializes it as a B-Tree node.
 *
 * Parameters
 * - bt: B-Tree file
 * - npage: Out parameter. Returns the number of the page that
 *          was allocated.
 * - type: Type of B-Tree node (PGTYPE_TABLE_INTERNAL, PGTYPE_TABLE_LEAF,
 *         PGTYPE_INDEX_INTERNAL, or PGTYPE_INDEX_LEAF)
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_newNode(BTree *bt, npage_t *npage, uint8_t type)
{
    chidb_Pager_allocatePage(bt->pager, npage);

    BTreeNode new_node = chidb_Btree_createNode(bt, *npage, type);
    return chidb_Btree_writeNode(bt, &new_node);
}


/* Initialize a B-Tree node
 *
 * Initializes a database page to contain an empty B-Tree node. The
 * database page is assumed to exist and to have been already allocated
 * by the pager.
 *
 * Parameters
 * - bt: B-Tree file
 * - npage: Database page where the node will be created.
 * - type: Type of B-Tree node (PGTYPE_TABLE_INTERNAL, PGTYPE_TABLE_LEAF,
 *         PGTYPE_INDEX_INTERNAL, or PGTYPE_INDEX_LEAF)
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_initEmptyNode(BTree *bt, npage_t npage, uint8_t type)
{
    BTreeNode node = chidb_Btree_createNode(bt, npage, type);
    return chidb_Btree_writeNode(bt, &node);
}


/* Read the contents of a cell
 *
 * Reads the contents of a cell from a BTreeNode and stores them in a BTreeCell.
 * This involves the following:
 *  1. Find out the offset of the requested cell.
 *  2. Read the cell from the in-memory page, and parse its
 *     contents (refer to The chidb File Format document for
 *     the format of cells).
 *
 * Parameters
 * - btn: BTreeNode where cell is contained
 * - ncell: Cell number
 * - cell: BTreeCell where contents must be stored.
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ECELLNO: The provided cell number is invalid
 */
int chidb_Btree_getCell(BTreeNode *btn, ncell_t ncell, BTreeCell *cell)
{
    if (ncell < 0 || ncell > btn->n_cells) {
        return CHIDB_ECELLNO;
    }

    uint16_t cell_offset = get2byte(btn->celloffset_array + ncell*2);
    uint8_t *cell_data = btn->page->data + cell_offset;
    READ_VARINT32(key, cell_data, 4)

    cell->type = btn->type;
    cell->key = (chidb_key_t)key;

    if (cell->type == PGTYPE_TABLE_INTERNAL) {
        uint32_t child_page = get4byte(cell_data);
        (cell->fields).tableInternal.child_page = child_page;
    } else if (cell->type == PGTYPE_INDEX_INTERNAL) {
        uint32_t child_page = get4byte(cell_data);
        uint32_t keyPk = get4byte(cell_data + 12);;
        (cell->fields).indexInternal.child_page = child_page;
        (cell->fields).indexInternal.keyPk = keyPk;
    } else if (cell->type == PGTYPE_TABLE_LEAF) {
        READ_VARINT32(data_size, cell_data, 0)
        (cell->fields).tableLeaf.data_size = data_size;
        (cell->fields).tableLeaf.data = cell_data + 8;
    } else if (cell->type == PGTYPE_INDEX_LEAF) {
        uint32_t keyPk = get4byte(cell_data + 8);;
        (cell->fields).indexLeaf.keyPk = keyPk;
    } else {
        // TODO
    }

    return CHIDB_OK;
}


/* Insert a new cell into a B-Tree node
 *
 * Inserts a new cell into a B-Tree node at a specified position ncell.
 * This involves the following:
 *  1. Add the cell at the top of the cell area. This involves "translating"
 *     the BTreeCell into the chidb format (refer to The chidb File Format
 *     document for the format of cells).
 *  2. Modify cells_offset in BTreeNode to reflect the growth in the cell area.
 *  3. Modify the cell offset array so that all values in positions >= ncell
 *     are shifted one position forward in the array. Then, set the value of
 *     position ncell to be the offset of the newly added cell.
 *
 * This function assumes that there is enough space for this cell in this node.
 *
 * Parameters
 * - btn: BTreeNode to insert cell in
 * - ncell: Cell number
 * - cell: BTreeCell to insert.
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ECELLNO: The provided cell number is invalid
 */
int chidb_Btree_insertCell(BTreeNode *btn, ncell_t ncell, BTreeCell *cell)
{
    assert(cell->type == btn->type);
    if (ncell < 0 || ncell > btn->n_cells + 1) {
        return CHIDB_ECELLNO;
    }

    // pointer to start of cells
    uint8_t *cells_offset = btn->page->data + btn->cells_offset;
    // write cell
    if (cell->type == PGTYPE_TABLE_INTERNAL) {
        putVarint32(cells_offset - 4, cell->key);
        put4byte(cells_offset - 8, (cell->fields).tableInternal.child_page);
        btn->cells_offset -= 8;
    } else if (cell->type == PGTYPE_INDEX_INTERNAL) {
        put4byte(cells_offset - 4, (cell->fields).indexInternal.keyPk);
        put4byte(cells_offset - 8, cell->key);
        btn->page->data[btn->cells_offset - 9] = 0x04;
        btn->page->data[btn->cells_offset - 10] = 0x04;
        btn->page->data[btn->cells_offset - 11] = 0x03;
        btn->page->data[btn->cells_offset - 12] = 0x0B;
        put4byte(cells_offset - 16, (cell->fields).indexInternal.child_page);
        btn->cells_offset -= 16;
    } else if (cell->type == PGTYPE_TABLE_LEAF) {
        uint32_t data_size = (cell->fields).tableLeaf.data_size;
        memcpy(cells_offset - data_size, (cell->fields).tableLeaf.data, data_size);
        putVarint32(cells_offset - data_size - 4, cell->key);
        putVarint32(cells_offset - data_size - 8, data_size);
        btn->cells_offset -= (data_size + 8);
    } else if (cell->type == PGTYPE_INDEX_LEAF) {
        put4byte(cells_offset - 4, (cell->fields).indexInternal.keyPk);
        put4byte(cells_offset - 8, cell->key);
        btn->page->data[btn->cells_offset - 9] = 0x04;
        btn->page->data[btn->cells_offset - 10] = 0x04;
        btn->page->data[btn->cells_offset - 11] = 0x03;
        btn->page->data[btn->cells_offset - 12] = 0x0B;
        btn->cells_offset -= 12;
    } else {
        // TODO
    }

    // write cell offset
    uint8_t *new_cell_offset = btn->celloffset_array + (2 * ncell);
    size_t num_bytes_to_shift = (btn->n_cells - ncell) * 2;
    memmove(new_cell_offset + 2, new_cell_offset, num_bytes_to_shift);
    put2byte(new_cell_offset, btn->cells_offset); // our new cell is at the top

    btn->n_cells++;
    btn->free_offset += 2;
    return CHIDB_OK;
}

/* Find an entry in a table B-Tree
 *
 * Finds the data associated for a given key in a table B-Tree
 *
 * Parameters
 * - bt: B-Tree file
 * - nroot: Page number of the root node of the B-Tree we want search in
 * - key: Entry key
 * - data: Out-parameter where a copy of the data must be stored
 * - size: Out-parameter where the number of bytes of data must be stored
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ENOTFOUND: No entry with the given key was found
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
// TODO: error handling, and switch to iterative version from textbook (p.489)
int chidb_Btree_find(BTree *bt, npage_t nroot, chidb_key_t key, uint8_t **data, uint16_t *size)
{
    chilog(TRACE, "searching for key %d at node %d", key, nroot);
    chidb_Btree_print(bt, nroot, chidb_BTree_stringPrinter, true);
    BTreeNode *btn;
    chidb_Btree_getNodeByPage(bt, nroot, &btn);

    // if it's a leaf, search cells with key == key
    if (btn->type == PGTYPE_TABLE_LEAF) {
        for (int i = 0; i < btn->n_cells; i++) {
           BTreeCell btc;
           chilog(TRACE, "\tleaf cell %d has value %d", i, btc.key);
           chidb_Btree_getCell(btn, i, &btc);
            if (btc.key == key) {
                *size = btc.fields.tableLeaf.data_size;
                *data = btc.fields.tableLeaf.data;
                return CHIDB_OK;
            }
        }
        return CHIDB_ENOTFOUND;
    // otherwise, recursively call find on the correct child
    } else if (btn->type == PGTYPE_TABLE_INTERNAL) {
        for (int i = 0; i < btn->n_cells; i++) {
            BTreeCell btc;
            chidb_Btree_getCell(btn, i, &btc);
            chilog(TRACE, "\tinternal cell %d has value %d", i, btc.key);
            if (key <= btc.key) {
                return chidb_Btree_find(
                    bt, btc.fields.tableInternal.child_page, key, data, size);
            }
        }
        return chidb_Btree_find(bt, btn->right_page, key, data, size);
    } else { // something went wrong, why are we in an index tree?
        return CHIDB_ENOTFOUND;
    }
}



/* Insert an entry into a table B-Tree
 *
 * This is a convenience function that wraps around chidb_Btree_insert.
 * It takes a key and data, and creates a BTreeCell that can be passed
 * along to chidb_Btree_insert.
 *
 * Parameters
 * - bt: B-Tree file
 * - nroot: Page number of the root node of the B-Tree we want to insert
 *          this entry in.
 * - key: Entry key
 * - data: Pointer to data we want to insert
 * - size: Number of bytes of data
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EDUPLICATE: An entry with that key already exists
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_insertInTable(BTree *bt, npage_t nroot, chidb_key_t key, uint8_t *data, uint16_t size)
{
    BTreeCell btc = {
        .type = PGTYPE_TABLE_LEAF,
        .key = key,
        .fields.tableLeaf.data_size = size,
        .fields.tableLeaf.data = data
    };

    return chidb_Btree_insert(bt, nroot, &btc);
}


/* Insert an entry into an index B-Tree
 *
 * This is a convenience function that wraps around chidb_Btree_insert.
 * It takes a KeyIdx and a KeyPk, and creates a BTreeCell that can be passed
 * along to chidb_Btree_insert.
 *
 * Parameters
 * - bt: B-Tree file
 * - nroot: Page number of the root node of the B-Tree we want to insert
 *          this entry in.
 * - keyIdx: See The chidb File Format.
 * - keyPk: See The chidb File Format.
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EDUPLICATE: An entry with that key already exists
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_insertInIndex(BTree *bt, npage_t nroot, chidb_key_t keyIdx, chidb_key_t keyPk)
{
    BTreeCell btc = {
        .type = PGTYPE_INDEX_LEAF,
        .key = keyIdx,
        .fields.indexLeaf.keyPk = keyPk
    };

    return chidb_Btree_insert(bt, nroot, &btc);
}


// basic linked list data structure for keeping track of our traversal path
// when doing insertions
typedef struct ll_node {
    struct ll_node *prev;
    BTreeNode *val;
    struct ll_node *next;
} ll_node;

typedef struct ll {
    ll_node *head;
    ll_node *tail;
} ll;

// return true if there is enough room in the node to insert the cell without splitting
// see chidb file format document for details
bool is_insertable(BTreeNode *btn, BTreeCell *btc) {
    size_t num_bytes_available = btn->cells_offset - btn->free_offset;
    size_t num_bytes_needed = 2; // 2 bytes for the cell offset
    if (btc->type == PGTYPE_TABLE_LEAF) {
        num_bytes_needed += 8 + (btc->fields).tableLeaf.data_size;
    } else if (btc->type == PGTYPE_TABLE_INTERNAL) {
        num_bytes_needed += 8;
    } else if (btc->type == PGTYPE_INDEX_LEAF) {
        num_bytes_needed += 16;
    } else if (btc->type == PGTYPE_TABLE_INTERNAL) {
        num_bytes_needed += 12;
    } else {
        // TODO
    }
    chilog(TRACE, "bytes available: %d, needed: %d", num_bytes_available, num_bytes_needed);
    return num_bytes_available >= num_bytes_needed;
}

/* Insert a BTreeCell into a B-Tree
 *
 * The chidb_Btree_insert function handles b tree insertion of a new cell/record
 * in the same spirit as the pseudocode of fig 11.15 of the sailboat textbook:
 * First we search for the leaf page which should contain our new BTreeCell
 * using the same logic as chidb_Btree_find, but while keeping track of the
 * encountered tree nodes along the way in a linked list.
 * Once we have found the leaf node, we keep splitting and moving back up the
 * traversed path towards the root until we find a node that is not full where
 * we can insert without splitting. In the simplest case, this would be the leaf
 * node where we can simply call insertNonFull without doing any backtracking up
 * the tree. If we end up reaching the root, then we split the root and create
 * a new root node.
 *
 * Parameters
 * - bt: B-Tree file
 * - nroot: Page number of the root node of the B-Tree we want to insert
 *          this cell in.
 * - btc: BTreeCell to insert into B-Tree
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EDUPLICATE: An entry with that key already exists
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_insert(BTree *bt, npage_t nroot, BTreeCell *to_insert)
{
    chilog(TRACE, "inserting key %d at node %d", to_insert->key, nroot);
    // chidb_Btree_print(bt, nroot, chidb_BTree_stringPrinter, true);
    BTreeNode *btn;
    chidb_Btree_getNodeByPage(bt, nroot, &btn);
    ll_node current = { .val=btn };
    ll path = { .head=&current, .tail=&current };

    // find leaf node to insert into and keep track of path
    while (!(btn->type == PGTYPE_TABLE_LEAF || btn->type == PGTYPE_INDEX_LEAF)) {
        BTreeNode *next = NULL;
        for (int i = 0; i < btn->n_cells; i++) {
            BTreeCell btc;
            chidb_Btree_getCell(btn, i, &btc);
            chilog(TRACE, "\tinternal cell %d has value %d", i, btc.key);
            if (to_insert->key <= btc.key) {
                chidb_Btree_getNodeByPage(
                    bt, btc.fields.tableInternal.child_page, &next);
                break;
            }
        }
        if (next == NULL) { 
            chidb_Btree_getNodeByPage(bt, btn->right_page, &next);
        }
        btn = next;
        current.val = btn;
        (path.tail)->next = &current;
        current.prev = path.tail;
        path.tail = &current;
    }

    npage_t prev_right = -1;
    // for a more balanced split, should split by space instead of # of cells
    while (!(is_insertable(btn, to_insert))) {
        // take in to account our not yet inserted cell when getting median
        int median_index = btn->n_cells / 2;

        // create an array of pointers to cells of the overfull node (i.e.
        // the current cells + the cell we want to inserted), sorted by key
        BTreeCell overfull_node[btn->n_cells + 1];
        bool inserted = false;
        for (int i = 0; i < btn->n_cells; i++) {
            BTreeCell btc;
            chidb_Btree_getCell(btn, i, &btc);
            chilog(TRACE, "\tinternal cell %d has value %d", i, btc.key);
            if (to_insert->key < btc.key && !inserted) {
                inserted = true;
                overfull_node[i] = *to_insert;
                overfull_node[i + 1] = btc;
                if (prev_right != -1) { // in internal node
                    (overfull_node[i + 1].fields).tableInternal.child_page = prev_right;
                }
            } else {
                overfull_node[inserted ? i + 1 : i] = btc;
            }
        }
        if (!inserted) {
            overfull_node[btn->n_cells] = *to_insert;
        }

        // here left/right child refers to the two split nodes of the overfull node -
        // left contains the smaller values and right contains the larger values.
        // we overwrite the overfull node with the left node, and create a new page
        // to store the right node
        BTreeNode left_child = chidb_Btree_createNode(bt, btn->page->npage, btn->type);

        npage_t right_child_npage;
        chidb_Pager_allocatePage(bt->pager, &right_child_npage);
        BTreeNode right_child = chidb_Btree_createNode(bt, right_child_npage, btn->type);

        for (int i = 0; i < median_index; i++) {
            chidb_Btree_insertCell(&left_child, i, overfull_node + i);
        }
        if (btn->type == PGTYPE_TABLE_LEAF) {
            chidb_Btree_insertCell(&left_child, median_index, overfull_node + median_index);
        }
        for (int i = median_index + 1; i <= btn->n_cells; i++) {
            chidb_Btree_insertCell(&right_child, i - median_index - 1, overfull_node + i);
        }

        to_insert = malloc(sizeof(BTreeCell));
        to_insert->type = PGTYPE_TABLE_INTERNAL;
        to_insert->key = overfull_node[median_index].key;
        // we are inserting a node into parent whose child is the left split node
        // (the right split node is set later, since we need the parent node)
        (to_insert->fields).tableInternal.child_page = btn->page->npage;
        if (btn->type == PGTYPE_TABLE_INTERNAL) {
            right_child.right_page = left_child.right_page;
            left_child.right_page = (overfull_node[median_index].fields).tableInternal.child_page;    
        }
        prev_right = right_child_npage;

        // write the new split nodes, and insert into the parent
        chidb_Btree_writeNode(bt, &left_child);
        chidb_Btree_writeNode(bt, &right_child);

        chilog(INFO, "left split (page %d):", left_child.page->npage);
        chidb_Btree_print(bt, left_child.page->npage, chidb_BTree_stringPrinter, true);
        chilog(INFO, "right split (page %d):", right_child.page->npage);
        chidb_Btree_print(bt, right_child.page->npage, chidb_BTree_stringPrinter, true);

        // we are at the root, so create a new root (which is guaranteed to have room)
        if (path.tail->val == btn) {
            npage_t nroot;
            chidb_Pager_allocatePage(bt->pager, &nroot);
            chilog(TRACE, "creating new root in page %d", nroot);
            BTreeNode new_root = chidb_Btree_createNode(bt, nroot, PGTYPE_TABLE_INTERNAL);
            int result = chidb_Btree_insertNonFull(bt, &new_root, to_insert, prev_right);
            chidb_Btree_print(bt, nroot, chidb_BTree_stringPrinter, true);
            return result;
        }
        path.tail = (path.tail)->prev;
        chidb_Btree_freeMemNode(bt, btn);
        btn = (path.tail)->val;
    }
    int result = chidb_Btree_insertNonFull(bt, btn, to_insert, prev_right);
    chidb_Btree_print(bt, nroot, chidb_BTree_stringPrinter, true);
    return result;
}

/* Insert a BTreeCell into a non-full leaf B-Tree node
 *
 * chidb_Btree_insertNonFull inserts a BTreeCell into a node that is
 * assumed not to be full (i.e., does not require splitting),
 * in the appropriate position according to its key. It will also update the
 * child pointers for internal nodes after insertion using the right child
 * parameter
 *
 * Parameters
 * - bt: B-Tree file
 * - btn: Leaf node we want to insert into
 * - btc: BTreeCell to insert into B-Tree
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_EDUPLICATE: An entry with that key already exists
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_insertNonFull(BTree *bt, BTreeNode *btn, BTreeCell *to_insert, npage_t right_child)
{
    int insertion_index = -1;
    for (int i = 0; i < btn->n_cells; i++) {
        BTreeCell btc;
        chidb_Btree_getCell(btn, i, &btc);
        chilog(TRACE, "\tinternal cell %d has value %d", i, btc.key);
        if (to_insert->key < btc.key) {
            insertion_index = i;
        } else if (to_insert->key == btc.key) {
            return CHIDB_EDUPLICATE;
        }
    }
    insertion_index = insertion_index == -1 ? btn->n_cells : insertion_index;
    if (btn->type == PGTYPE_TABLE_INTERNAL) { // update pointers
        if (insertion_index == btn->n_cells) { // appended as largest cell
            btn->right_page = right_child;
        } else {
            BTreeCell btc;
            chidb_Btree_getCell(btn, insertion_index, &btc);
            btc.fields.tableInternal.child_page = right_child;
        }
    }
    int result = chidb_Btree_insertCell(btn, insertion_index, to_insert);
    return result == CHIDB_OK ? chidb_Btree_writeNode(bt, btn) : result;
}
