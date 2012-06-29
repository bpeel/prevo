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
