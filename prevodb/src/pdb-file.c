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

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>

#include "pdb-file.h"

gboolean
pdb_file_open (PdbFile *file,
               const char *filename,
               PdbFileMode mode,
               GError **error)
{
  file->file = fopen (filename,
                      mode == PDB_FILE_MODE_WRITE ? "wb" : "rb");

  if (file->file == NULL)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", filename, strerror (errno));
      return FALSE;
    }

  file->pos = 0;
  file->filename = g_strdup (filename);

  return TRUE;
}

gboolean
pdb_file_write (PdbFile *file,
                const void *data,
                size_t size,
                GError **error)
{
  if (fwrite (data, 1, size, file->file) != size)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", file->filename, strerror (errno));
      return FALSE;
    }
  else
    {
      file->pos += size;
      return TRUE;
    }
}

gboolean
pdb_file_write_8 (PdbFile *file,
                  guint8 val,
                  GError **error)
{
  return pdb_file_write (file, &val, sizeof (val), error);
}

gboolean
pdb_file_write_16 (PdbFile *file,
                   guint16 val,
                   GError **error)
{
  val = GUINT16_TO_LE (val);

  return pdb_file_write (file, &val, sizeof (val), error);
}

gboolean
pdb_file_write_32 (PdbFile *file,
                   guint32 val,
                   GError **error)
{
  val = GUINT32_TO_LE (val);

  return pdb_file_write (file, &val, sizeof (val), error);
}

gboolean
pdb_file_read (PdbFile *file,
               void *data,
               size_t size,
               GError **error)
{
  errno = 0;

  if (fread (data, 1, size, file->file) != size)
    {
      if (errno)
        g_set_error (error,
                     G_FILE_ERROR,
                     g_file_error_from_errno (errno),
                     "%s: %s", file->filename, strerror (errno));
      else
        g_set_error (error,
                     G_FILE_ERROR,
                     G_FILE_ERROR_IO,
                     "%s: Unexpected EOF", file->filename);

      return FALSE;
    }
  else
    {
      file->pos += size;
      return TRUE;
    }
}

gboolean
pdb_file_read_8 (PdbFile *file,
                 guint8 *val,
                 GError **error)
{
  return pdb_file_read (file, val, sizeof (*val), error);
}

gboolean
pdb_file_read_16 (PdbFile *file,
                  guint16 *val,
                  GError **error)
{
  if (pdb_file_read (file, val, sizeof (*val), error))
    {
      *val = GUINT16_FROM_LE (*val);
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
pdb_file_read_32 (PdbFile *file,
                  guint32 *val,
                  GError **error)
{
  if (pdb_file_read (file, val, sizeof (*val), error))
    {
      *val = GUINT32_FROM_LE (*val);
      return TRUE;
    }
  else
    return FALSE;
}

gboolean
pdb_file_seek (PdbFile *file,
               long offset,
               int whence,
               GError **error)
{
  if (fseek (file->file, offset, whence) == 0)
    {
      switch (whence)
        {
        case SEEK_SET:
          file->pos = offset;
          break;

        case SEEK_CUR:
          file->pos += offset;
          break;

        default:
          g_assert_not_reached ();
        }

      return TRUE;
    }
  else
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", file->filename, strerror (errno));

      return FALSE;
    }
}

gboolean
pdb_file_close (PdbFile *file,
                GError **error)
{
  gboolean ret = TRUE;

  if (fclose (file->file) == EOF)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", file->filename, strerror (errno));
      ret = FALSE;
    }
  else
    ret = TRUE;

  g_free (file->filename);

  return ret;
}
