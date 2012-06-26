#include "config.h"

#include <glib/gstdio.h>
#include <string.h>
#include <errno.h>

#include "pdb-mkdir.h"

gboolean
pdb_try_mkdir (const char *dir,
               GError **error)
{
  if (g_mkdir (dir, 0777) == -1 && errno != EEXIST)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", dir, strerror (errno));
      return FALSE;
    }

  return TRUE;
}
