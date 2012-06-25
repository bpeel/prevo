#include "config.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "pdb-revo.h"
#include "pdb-error.h"

struct _PdbRevo
{
  char *zip_file;
};

PdbRevo *
pdb_revo_new (const char *filename,
              GError **error)
{
  PdbRevo *revo;

  revo = g_slice_new (PdbRevo);
  revo->zip_file = g_strdup (filename);

  return revo;
}

gboolean
pdb_revo_parse_xml (PdbRevo *revo,
                    XML_Parser parser,
                    const char *filename,
                    GError **error)
{
  gchar *argv[] = { "unzip", "-p", revo->zip_file, (char *) filename, NULL };
  char buf[512];
  gboolean ret = TRUE;
  int child_out, child_err;
  GPid child_pid;
  int got;
  GString *error_buf;
  gboolean in_end = FALSE, err_end = FALSE;
  gboolean aborted = FALSE;
  int child_status;

  if (!g_spawn_async_with_pipes (NULL, /* working dir */
                                 argv,
                                 NULL, /* env */
                                 G_SPAWN_SEARCH_PATH |
                                 G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, /* child_setup */
                                 NULL, /* user_data for child_setup */
                                 &child_pid,
                                 NULL, /* stdin */
                                 &child_out, /* stdout */
                                 &child_err, /* stderr */
                                 error))
    return FALSE;

  error_buf = g_string_new (NULL);

  do
    {
      GPollFD fds[2];
      int nfds = 0;
      int i;

      if (!err_end)
        {
          fds[nfds].fd = child_err;
          fds[nfds].events = G_IO_IN | G_IO_ERR | G_IO_HUP;
          fds[nfds].revents = 0;
          nfds++;
        }

      if (!in_end)
        {
          fds[nfds].fd = child_out;
          fds[nfds].events = G_IO_IN | G_IO_ERR | G_IO_HUP;
          fds[nfds].revents = 0;
          nfds++;
        }

      if ((nfds = g_poll (fds, nfds, -1)) == -1)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "poll: %s",
                       strerror (errno));
          ret = FALSE;
          goto done;
        }
      else
        for (i = 0; i < nfds; i++)
          if (fds[i].fd == child_out &&
              (fds[i].revents & (G_IO_IN | G_IO_ERR | G_IO_HUP)))
            {
              got = read (child_out, buf, sizeof (buf));

              if (got == -1)
                {
                  g_set_error (error,
                               G_FILE_ERROR,
                               g_file_error_from_errno (errno),
                               "%s",
                               strerror (errno));
                  ret = FALSE;
                  goto done;
                }
              else if (XML_Parse (parser, buf, got, got == 0) ==
                       XML_STATUS_ERROR)
                {
                  pdb_error_from_parser (parser, filename, error);
                  ret = FALSE;
                  if (XML_GetErrorCode (parser) == XML_ERROR_ABORTED)
                    aborted = TRUE;
                  goto done;
                }
              else if (got == 0)
                in_end = TRUE;
            }
          else if (fds[i].fd == child_err &&
                   (fds[i].revents & (G_IO_IN | G_IO_ERR | G_IO_HUP)))

            {
              g_string_set_size (error_buf, error_buf->len + 512);

              got = read (child_err, error_buf->str, 512);

              if (got == -1)
                {
                  g_set_error (error,
                               G_FILE_ERROR,
                               g_file_error_from_errno (errno),
                               "%s",
                               strerror (errno));
                  ret = FALSE;
                  goto done;
                }
              else
                {
                  g_string_set_size (error_buf, error_buf->len - 512 + got);
                  if (got == 0)
                    err_end = TRUE;
                }
            }
    } while (!err_end || !in_end);

 done:

  close (child_out);
  close (child_err);

  while (waitpid (child_pid, &child_status, 0 /* options */) == -1 &&
         errno == EINTR);

  if (!aborted && (!WIFEXITED (child_status) || WEXITSTATUS (child_status)))
    {
      char *end = strchr (error_buf->str, '\n');

      if (!ret && error)
        g_clear_error (error);

      if (end == NULL)
        end = error_buf->str + error_buf->len;
      else
        {
          while (end > error_buf->str && g_ascii_isspace (end[-1]))
            end--;
          *end = '\0';
        }

      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_UNZIP_FAILED,
                   "%s",
                   error_buf->str[0] ?
                   error_buf->str :
                   "Unzip failed");
      ret = FALSE;
    }

  g_string_free (error_buf, TRUE);

  return ret;
}

void
pdb_revo_free (PdbRevo *revo)
{
  g_free (revo->zip_file);
  g_slice_free (PdbRevo, revo);
}
