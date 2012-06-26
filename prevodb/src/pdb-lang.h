#ifndef PDB_LANG_H
#define PDB_LANG_H

#include <glib.h>

#include "pdb-revo.h"
#include "pdb-trie.h"

typedef struct _PdbLang PdbLang;

PdbLang *
pdb_lang_new (PdbRevo *revo,
              GError **error);

PdbTrieBuilder *
pdb_lang_get_trie (PdbLang *lang,
                   const char *lang_code);

void
pdb_lang_free (PdbLang *lang);

gboolean
pdb_lang_save (PdbLang *lang,
               const char *dir,
               GError **error);

#endif /* PDB_LANG_H */
