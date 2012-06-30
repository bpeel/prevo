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
