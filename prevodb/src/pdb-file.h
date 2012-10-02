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

#ifndef PDB_FILE_H
#define PDB_FILE_H

#include <glib.h>
#include <stdio.h>

typedef struct
{
  char *filename;
  FILE *out;
  size_t pos;
} PdbFile;

gboolean
pdb_file_open (PdbFile *file,
               const char *filename,
               GError **error);

gboolean
pdb_file_write (PdbFile *file,
                const void *data,
                size_t size,
                GError **error);

gboolean
pdb_file_write_8 (PdbFile *file,
                  guint8 val,
                  GError **error);

gboolean
pdb_file_write_16 (PdbFile *file,
                   guint16 val,
                   GError **error);

gboolean
pdb_file_write_32 (PdbFile *file,
                   guint32 val,
                   GError **error);

gboolean
pdb_file_seek (PdbFile *file,
               long offset,
               int whence,
               GError **error);
gboolean
pdb_file_close (PdbFile *file,
                GError **error);

#endif /* PDB_FILE_H */
