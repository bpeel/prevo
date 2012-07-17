/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2012  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PDB_TRIE_H
#define PDB_TRIE_H

#include <glib.h>

typedef struct _PdbTrie PdbTrie;

typedef void (* PdbTrieFreeDataCb) (void *data,
                                    void *user_data);
typedef void (* PdbTrieGetReferenceCb) (void *data,
                                        int *article_num,
                                        int *mark_num,
                                        void *user_data);

PdbTrie *
pdb_trie_new (PdbTrieFreeDataCb free_data_cb,
              PdbTrieGetReferenceCb get_reference_cb,
              void *user_data);

void
pdb_trie_add_word (PdbTrie *trie,
                   const char *word,
                   const char *display_representation,
                   void *data);

void
pdb_trie_compress (PdbTrie *trie,
                   guint8 **data,
                   int *len);

void
pdb_trie_free (PdbTrie *trie);

gboolean
pdb_trie_is_empty (PdbTrie *trie);

#endif /* PDB_TRIE_H */
