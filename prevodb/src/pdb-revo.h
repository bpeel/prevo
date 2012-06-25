#ifndef PDB_REVO_H
#define PDB_REVO_H

#include <glib.h>
#include <expat.h>

#include "pdb-revo.h"

typedef struct _PdbRevo PdbRevo;

PdbRevo *
pdb_revo_new (const char *filename,
              GError **error);

gboolean
pdb_revo_parse_xml (PdbRevo *revo,
                    XML_Parser parser,
                    const char *filename,
                    GError **error);

char **
pdb_revo_list_files (PdbRevo *revo,
                     const char *glob,
                     GError **error);

void
pdb_revo_free (PdbRevo *revo);

#endif /* PDB_REVO_H */
