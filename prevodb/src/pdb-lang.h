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

#ifndef PDB_LANG_H
#define PDB_LANG_H

#include <glib.h>

#include "pdb-revo.h"
#include "pdb-trie.h"

typedef struct _PdbLang PdbLang;

PdbLang *
pdb_lang_new (PdbRevo *revo,
              PdbTrieFreeDataCb free_data_cb,
              PdbTrieGetReferenceCb get_reference_cb,
              void *user_data,
              GError **error);

PdbTrie *
pdb_lang_get_trie (PdbLang *lang,
                   const char *lang_code);

void
pdb_lang_free (PdbLang *lang);

gboolean
pdb_lang_save (PdbLang *lang,
               const char *dir,
               GError **error);

#endif /* PDB_LANG_H */
