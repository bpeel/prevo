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
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "pdb-groff.h"

static const char *
pdb_groff_args[] =
  {
    "/bin/sh", "-c", "groff -Tutf8 -Kutf8 -mandoc | less -s", NULL
  };

struct _PdbGroff
{
  GPid pid;
  FILE *stdin_stream;
};

static void
kill_process (GPid pid)
{
  int status;

  if (waitpid (pid, &status, WNOHANG) <= 0)
    {
      kill (pid, SIGTERM);

      while (waitpid (pid, &status, 0) == -1 &&
             errno == EINTR);
    }
}

PdbGroff *
pdb_groff_new (GError **error)
{
  GPid pid;
  int stdin_fd;

  if (g_spawn_async_with_pipes (NULL, /* working_dir */
                                (char **) pdb_groff_args,
                                NULL, /* envp */
                                G_SPAWN_DO_NOT_REAP_CHILD,
                                NULL, /* child_setup_func */
                                NULL, /* user_data */
                                &pid,
                                &stdin_fd,
                                NULL, /* standard_output */
                                NULL, /* standard_error */
                                error))
    {
      FILE *stdin_stream;

      stdin_stream = fdopen (stdin_fd, "w");

      if (stdin_stream == NULL)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "Error starting groff: %s",
                       strerror (errno));
          close (stdin_fd);
          kill_process (pid);

          return NULL;
        }
      else
        {
          PdbGroff *groff = g_slice_new (PdbGroff);

          groff->stdin_stream = stdin_stream;
          groff->pid = pid;

          return groff;
        }
    }
  else
    return NULL;
}

gboolean
pdb_groff_display (PdbGroff *groff,
                   GError **error)
{
  int wait_ret;
  int status;
  int close_ret;

  close_ret = fclose (groff->stdin_stream);
  groff->stdin_stream = NULL;

  if (close_ret == EOF)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "Error writing to groff: %s", strerror (errno));
      return FALSE;
    }

  while ((wait_ret = waitpid (groff->pid, &status, 0)) == -1 &&
         errno == EINTR);

  groff->pid = 0;

  if (wait_ret == -1)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "Error waiting for groff: %s", strerror (errno));
      return FALSE;
    }

  if (status != 0)
    {
      g_set_error (error,
                   PDB_GROFF_ERROR,
                   PDB_GROFF_ERROR_STATUS,
                   "Failed to run groff");
      return FALSE;
    }

  return TRUE;
}

FILE *
pdb_groff_get_output (PdbGroff *groff)
{
  return groff->stdin_stream;
}

void
pdb_groff_free (PdbGroff *groff)
{
  if (groff->stdin_stream)
    fclose (groff->stdin_stream);

  if (groff->pid)
    kill_process (groff->pid);

  g_slice_free (PdbGroff, groff);
}

GQuark
pdb_groff_error_quark (void)
{
  return g_quark_from_static_string ("pdb-groff-error-quark");
}
