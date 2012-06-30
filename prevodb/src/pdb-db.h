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

#ifndef PDB_DB_H
#define PDB_DB_H

#include <glib.h>

#include "pdb-revo.h"

typedef struct _PdbDb PdbDb;

PdbDb *
pdb_db_new (PdbRevo *revo,
            GError **error);

gboolean
pdb_db_save (PdbDb *db,
             const char *dir,
             GError **error);

void
pdb_db_free (PdbDb *db);

#endif /* PDB_DB_H */
