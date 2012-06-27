#ifndef PDB_STRCMP_H
#define PDB_STRCMP_H

#include <glib.h>

int
pdb_strcmp (const char *a,
            const char *b);

int
pdb_strcmp_ch (gunichar a,
               gunichar b);

#endif /* PDB_STRCMP_H */
