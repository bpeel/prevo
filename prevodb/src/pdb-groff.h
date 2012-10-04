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

#ifndef PDB_GROFF_H
#define PDB_GROFF_H

#include <glib.h>
#include <stdio.h>

#define PDB_GROFF_ERROR (pdb_groff_error_quark ())

typedef struct _PdbGroff PdbGroff;

typedef enum
{
  PDB_GROFF_ERROR_STATUS
} PbbGroffError;

PdbGroff *
pdb_groff_new (GError **error);

gboolean
pdb_groff_display (PdbGroff *groff,
                   GError **error);

FILE *
pdb_groff_get_output (PdbGroff *groff);

void
pdb_groff_free (PdbGroff *groff);

GQuark
pdb_groff_error_quark (void);

#endif /* PDB_GROFF_H */
