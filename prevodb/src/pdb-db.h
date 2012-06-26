#ifndef PDB_DB_H
#define PDB_DB_H

#include <glib.h>

#include "pdb-revo.h"

typedef struct _PdbDb PdbDb;

PdbDb *
pdb_db_new (PdbRevo *revo,
            GError **error);

void
pdb_db_free (PdbDb *db);

#endif /* PDB_DB_H */
