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
typedef struct _PdbRevoFile PdbRevoFile;

PdbRevo *
pdb_revo_new (const char *filename,
              GError **error);

PdbRevoFile *
pdb_revo_open (PdbRevo *revo,
               const char *filename,
               GError **error);

gboolean
pdb_revo_read (PdbRevoFile *file,
               char *buf,
               size_t *buflen,
               GError **error);

void
pdb_revo_close (PdbRevoFile *file);

char **
pdb_revo_list_files (PdbRevo *revo,
                     const char *glob,
                     GError **error);

void
pdb_revo_free (PdbRevo *revo);

#endif /* PDB_REVO_H */
