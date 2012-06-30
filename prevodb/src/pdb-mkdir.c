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

#include <glib/gstdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "pdb-mkdir.h"

gboolean
pdb_try_mkdir (GError **error,
               ...)
{
  GString *full_name = g_string_new (NULL);
  gboolean ret = TRUE;
  const char *part;
  va_list ap;

  va_start (ap, error);

  while ((part = va_arg (ap, const char *)))
    {
      if (full_name->len > 0)
        g_string_append_c (full_name, G_DIR_SEPARATOR);
      g_string_append (full_name, part);

      if (g_mkdir (full_name->str, 0777) == -1 && errno != EEXIST)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "%s: %s",
                       full_name->str,
                       strerror (errno));
          ret = FALSE;
          break;
        }
    }

  va_end (ap);

  g_string_free (full_name, TRUE);

  return ret;
}
