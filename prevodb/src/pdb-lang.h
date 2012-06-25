#ifndef PDB_LANG_H
#define PDB_LANG_H

#include <glib.h>

#include "pdb-revo.h"

typedef struct _PdbLang PdbLang;

PdbLang *
pdb_lang_new (PdbRevo *revo,
              GError **error);

void
pdb_lang_free (PdbLang *lang);

#endif /* PDB_LANG_H */
