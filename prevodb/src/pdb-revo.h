#ifndef PDB_REVO_H
#define PDB_REVO_H

#include <glib.h>

#include "pdb-revo.h"

typedef struct _PdbRevo PdbRevo;

typedef enum
{
  PDB_REVO_READ_STATUS_OK,
  PDB_REVO_READ_STATUS_ABORT,
  PDB_REVO_READ_STATUS_ERROR
} PdbRevoReadStatus;

typedef PdbRevoReadStatus
(* PdbRevoReadCb) (const char *buf,
                   int len,
                   gboolean end,
                   void *user_data,
                   GError **error);

PdbRevo *
pdb_revo_new (const char *filename,
              GError **error);

gboolean
pdb_revo_parse_file (PdbRevo *revo,
                     const char *filename,
                     PdbRevoReadCb func,
                     void *user_data,
                     GError **error);

char **
pdb_revo_list_files (PdbRevo *revo,
                     const char *glob,
                     GError **error);

void
pdb_revo_free (PdbRevo *revo);

#endif /* PDB_REVO_H */
