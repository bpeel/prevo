#ifndef PDB_MKDIR_H
#define PDB_MKDIR_H

#include <glib.h>

gboolean
pdb_try_mkdir (GError **error,
               ...);

#endif /* PDB_MKDIR_H */
