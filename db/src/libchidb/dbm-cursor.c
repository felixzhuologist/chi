/*
 *  chidb - a didactic relational database management system
 *
 *  Database Machine cursors
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
#include "dbm-cursor.h"
#include <chidb/log.h>

int chidb_dbm_init_cursor(chidb_dbm_cursor_t *cursor, char *dbfile, chidb *db, npage_t root) {
  if (root != 1) {
    chilog(ERROR, "opening bt with root page != 1 not yet implemented");
    exit(1);
  }
  BTree *bt = malloc(sizeof(Btree));
  chidb_Btree_open(dbfile, db, &bt);
  cursor->bt = bt;
  (cursor->path).head = NULL;
  (cursor->path).tail = NULL;
  return CHIDB_OK;
}

int chidb_dbm_free_cursor(chidb_dbm_cursor_t *cursor) {
  // free cursor path
  ll_node *curr, *next;
  curr = (cursor->path).head;
  while (curr != NULL) {
    next = curr->next;
    cell_cursor *node = (cell_cursor*)(curr->val);
    chidb_Btree_freeMemNode(cursor->bt, node->btn);
    free(curr);
    curr = next;
  }

  free(&(cursor->path));
  chidb_Btree_close(cursor->bt);
  cursor->type = CURSOR_UNSPECIFIED;
  return CHIDB_OK;
}

bool chidb_dbm_rewind(chidb_dbm_cursor_t *cursor) {
  // TODO: keep track of root npage?
  BTreeNode *btn;
  chidb_Btree_getNodeByPage(cursor->bt, 1, &btn);
  cell_cursor *curr_cell_cursor = malloc(sizeof(cell_cursor));
  curr_cell_cursor->btn = btn;
  curr_cell_cursor->index = 0;

  ll path = cursor->path;
  while (btn->type == PGTYPE_TABLE_INTERNAL || btn->type == PGTYPE_INDEX_INTERNAL) {
    // save current btn into path linked list
    ll_node *curr = malloc(sizeof(ll_node));
    curr->prev = path.tail;
    curr->val = curr_cell_cursor;
    (path.tail)->next = curr;
    path.tail = curr;
    if (path.head == NULL) {
      path.head = curr;
    }

    // traverse down leftmost child
    BTreeCell btc;
    chidb_Btree_getCell(btn, 0, &btc);
    chidb_Btree_getNodeByPage(cursor->bt, 1, &btn);
    curr_cell_cursor = malloc(sizeof(cell_cursor));
    curr_cell_cursor->btn = btn;
    curr_cell_cursor->index = 0;
  }
  if (btn->n_cells == 0) { // we should only have an empty leaf if this is an empty tree
    return false;
  }
  ll_node *curr = malloc(sizeof(ll_node));
  curr->prev = path.tail;
  curr->val = curr_cell_cursor;
  (path.tail)->next = curr;
  path.tail = curr;
  return false;
}

bool chidb_dbm_next(chidb_dbm_cursor_t *cursor) {
  return true;
}

bool chidb_dbm_prev(chidb_dbm_cursor_t *cursor) {
  return true;
}

bool chidb_dbm_seek(chidb_dbm_cursor_t *cursor, chidb_key_t key) {
  return true;
}
