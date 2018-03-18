/*
 *  chidb - a didactic relational database management system
 *
 *  Database Machine cursors -- header
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


#ifndef DBM_CURSOR_H_
#define DBM_CURSOR_H_

#include <stdbool.h>
#include "chidbInt.h"
#include "btree.h"

// reference to a single cell, parametrized by a btn and an index into that btn
typedef struct cell_cursor
{
  BTreeNode *btn;
  ncell_t index;
} cell_cursor;

typedef enum chidb_dbm_cursor_type
{
    CURSOR_UNSPECIFIED,
    CURSOR_READ,
    CURSOR_WRITE
} chidb_dbm_cursor_type_t;

typedef struct chidb_dbm_cursor
{
    chidb_dbm_cursor_type_t type;
    // a linked list of cell cursors, going from root to leaf. O(logn) space
    // but allows for incr/decr in amortized O(1)... an easier implementation
    // might be to have pointers between consecutive leaf nodes so that the
    // "bottom layer" of the tree is doubly linked list
    ll path;
    BTree *bt;
} chidb_dbm_cursor_t;

int chidb_dbm_init_cursor(chidb_dbm_cursor_t *cursor, char *dbfile, chidb *db, npage_t root);
int chidb_dbm_free_cursor(chidb_dbm_cursor_t *cursor);
bool chidb_dbm_rewind(chidb_dbm_cursor_t *cursor); // return false if tree is empty
bool chidb_dbm_next(chidb_dbm_cursor_t *cursor); // return false if cursor is at the last row
bool chidb_dbm_prev(chidb_dbm_cursor_t *cursor); // return false if cursor is at the first row
bool chidb_dbm_seek(chidb_dbm_cursor_t *cursor, chidb_key_t key);

#endif /* DBM_CURSOR_H_ */
