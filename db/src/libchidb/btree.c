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

// Return a new empty BTreeNode
BTreeNode chidb_Btree_createNode(BTree *bt, npage_t npage, uint8_t type) {
    MemPage page = chidb_Pager_initMemPage(npage, bt->pager->page_size);
    bool is_leaf = type == PGTYPE_TABLE_LEAF || type ==  PGTYPE_INDEX_LEAF;
    uint16_t free_offset = is_leaf ? LEAFPG_CELLSOFFSET_OFFSET : INTPG_CELLSOFFSET_OFFSET;
    BTreeNode new_node = {
        .page=&page,
        .type=type,
        .free_offset=free_offset,
        .n_cells=0,
        .cells_offset=bt->pager->page_size,
    };
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
    if (f) {
        // TODO: use functions from pager.c to manage Pager?
        uint8_t buffer[100];
        int num_read;
        if ((num_read = fread(buffer, 1, 100, f)) < 100) {
            return CHIDB_ECORRUPTHEADER;
        }

        char *expected = "SQLite format 3";
        if (strncmp((char *)buffer, expected, strlen(expected))) {
            return CHIDB_ECORRUPTHEADER;
        }

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f) - 1;

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

    if ((*btn)->type == PGTYPE_TABLE_INTERNAL || (*btn)->type == PGTYPE_INDEX_INTERNAL) {
        (*btn)->right_page = (npage_t)get4byte(page->data + header_offset + PGHEADER_RIGHTPG_OFFSET);
        (*btn)->celloffset_array = page->data + header_offset + INTPG_CELLSOFFSET_OFFSET; 
    } else { // leaf page
        (*btn)->right_page = 0; // leaves have no right pointers
        (*btn)->celloffset_array = page->data + header_offset + LEAFPG_CELLSOFFSET_OFFSET;
    }

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
    chidb_Btree_syncNode(&new_node);
    return chidb_Pager_writePage(bt->pager, new_node.page);
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
    chidb_Btree_syncNode(&node);
    return chidb_Pager_writePage(bt->pager, node.page);
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
    if (ncell <= 0 || ncell > btn->n_cells) {
        return CHIDB_ECELLNO;
    }

    uint16_t cell_offset = get2byte(btn->celloffset_array + ncell*2);
    uint8_t *cell_data = btn->page->data + cell_offset;
    // chilog(INFO, "%d %d", cell_offset, btn->cells_offset);
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
        put4byte(cells_offset - 4, cell->key);
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
        put4byte(cells_offset - data_size - 4, cell->key);
        put4byte(cells_offset - data_size - 8, data_size);
        btn->cells_offset -= data_size - 8;
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
 * - CHIDB_ENOTFOUND: No entry with the given key way found
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_find(BTree *bt, npage_t nroot, chidb_key_t key, uint8_t **data, uint16_t *size)
{
    /* Your code goes here */

    return CHIDB_OK;
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
    /* Your code goes here */

    return CHIDB_OK;
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
    /* Your code goes here */

    return CHIDB_OK;
}


/* Insert a BTreeCell into a B-Tree
 *
 * The chidb_Btree_insert and chidb_Btree_insertNonFull functions
 * are responsible for inserting new entries into a B-Tree, although
 * chidb_Btree_insertNonFull is the one that actually does the
 * insertion. chidb_Btree_insert, however, first checks if the root
 * has to be split (a splitting operation that is different from
 * splitting any other node). If so, chidb_Btree_split is called
 * before calling chidb_Btree_insertNonFull.
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
int chidb_Btree_insert(BTree *bt, npage_t nroot, BTreeCell *btc)
{
    /* Your code goes here */

    return CHIDB_OK;
}

/* Insert a BTreeCell into a non-full B-Tree node
 *
 * chidb_Btree_insertNonFull inserts a BTreeCell into a node that is
 * assumed not to be full (i.e., does not require splitting). If the
 * node is a leaf node, the cell is directly added in the appropriate
 * position according to its key. If the node is an internal node, the
 * function will determine what child node it must insert it in, and
 * calls itself recursively on that child node. However, before doing so
 * it will check if the child node is full or not. If it is, then it will
 * have to be split first.
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
int chidb_Btree_insertNonFull(BTree *bt, npage_t npage, BTreeCell *btc)
{
    /* Your code goes here */

    return CHIDB_OK;
}


/* Split a B-Tree node
 *
 * Splits a B-Tree node N. This involves the following:
 * - Find the median cell in N.
 * - Create a new B-Tree node M.
 * - Move the cells before the median cell to M (if the
 *   cell is a table leaf cell, the median cell is moved too)
 * - Add a cell to the parent (which, by definition, will be an
 *   internal page) with the median key and the page number of M.
 *
 * Parameters
 * - bt: B-Tree file
 * - npage_parent: Page number of the parent node
 * - npage_child: Page number of the node to split
 * - parent_ncell: Position in the parent where the new cell will
 *                 be inserted.
 * - npage_child2: Out parameter. Used to return the page of the new child node.
 *
 * Return
 * - CHIDB_OK: Operation successful
 * - CHIDB_ENOMEM: Could not allocate memory
 * - CHIDB_EIO: An I/O error has occurred when accessing the file
 */
int chidb_Btree_split(BTree *bt, npage_t npage_parent, npage_t npage_child, ncell_t parent_ncell, npage_t *npage_child2)
{
    /* Your code goes here */

    return CHIDB_OK;
}

