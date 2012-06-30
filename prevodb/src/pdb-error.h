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

#ifndef PDB_ERROR_H
#define PDB_ERROR_H

#include <glib.h>

#include "pdb-xml.h"

#define PDB_ERROR (pdb_error_quark ())

typedef enum
{
  PDB_ERROR_BAD_FORMAT,
  PDB_ERROR_ABORTED,
  PDB_ERROR_UNZIP_FAILED,
  PDB_ERROR_PARSE
} PdbError;

GQuark
pdb_error_quark (void);

void
pdb_error_from_parser (PdbXmlParser *parser,
                       GError **error);

#endif /* PDB_ERROR_H */
