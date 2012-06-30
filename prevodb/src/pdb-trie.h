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

PdbTrie *
pdb_trie_new (void);

void
pdb_trie_add_word (PdbTrie *trie,
                   const char *word,
                   const char *display_representation,
                   int article_num,
                   int mark_num);

void
pdb_trie_compress (PdbTrie *trie,
                   guint8 **data,
                   int *len);

void
pdb_trie_free (PdbTrie *trie);

gboolean
pdb_trie_is_empty (PdbTrie *trie);

#endif /* PDB_TRIE_H */
