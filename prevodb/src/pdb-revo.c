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

static gboolean
pdb_revo_execute_command (PdbRevo *revo,
                          char **argv,
                          PdbRevoReadCb func,
                          void *user_data,
                          GError **error)
{
  char buf[512];
  gboolean ret = TRUE;
  int child_out, child_err;
  GPid child_pid;
  int got;
  GString *error_buf;
  gboolean in_end = FALSE, err_end = FALSE;
  gboolean aborted = FALSE;
  int child_status;
  PdbRevoReadStatus func_status;

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

      if (g_poll (fds, nfds, -1) == -1)
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
              else if ((func_status = func (buf,
                                            got,
                                            got == 0,
                                            user_data,
                                            error)) !=
                       PDB_REVO_READ_STATUS_OK)
                {
                  if (func_status == PDB_REVO_READ_STATUS_ABORT)
                    aborted = TRUE;
                  ret = FALSE;
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

typedef struct
{
  GString *line_buf;
  GPtrArray *files;
  gboolean in_list;
} PdbRevoListFilesData;

static PdbRevoReadStatus
pdb_revo_list_files_process_line (const char *line,
                                  PdbRevoListFilesData *data,
                                  GError **error)
{
  if (g_str_has_prefix (line, "---"))
    {
      data->in_list = !data->in_list;
      return PDB_REVO_READ_STATUS_OK;
    }

  if (data->in_list)
    {
      int i;

      /* Skip the first three columns */
      for (i = 0; i < 3; i++)
        {
          while (g_ascii_isspace (*line))
            line++;

          if (*line == '\0')
            goto error;

          while (!g_ascii_isspace (*line))
            line++;
        }

      /* Skip spaces to the filename */
      while (g_ascii_isspace (*line))
        line++;

      if (*line == '\0')
        goto error;

      g_ptr_array_add (data->files, g_strdup (line));
    }

  return PDB_REVO_READ_STATUS_OK;

 error:
  g_set_error (error,
               PDB_ERROR,
               PDB_ERROR_UNZIP_FAILED,
               "Unexepected data from unzip");

  return PDB_REVO_READ_STATUS_ERROR;
}

static PdbRevoReadStatus
pdb_revo_list_files_cb (const char *buf,
                        int len,
                        gboolean end,
                        void *user_data,
                        GError **error)
{
  PdbRevoListFilesData *data = user_data;
  char *line_end;
  int pos = 0;

  if (memchr (buf, '\0', len))
    {
      g_set_error (error, PDB_ERROR, PDB_ERROR_BAD_FORMAT,
                   "%s", "Embedded '\0' found in unzip listing");
      return PDB_REVO_READ_STATUS_ABORT;
    }

  g_string_append_len (data->line_buf, buf, len);

  while ((line_end = memchr (data->line_buf->str + pos,
                             '\n',
                             data->line_buf->len - pos)))
    {
      PdbRevoReadStatus status;

      *line_end = '\0';

      if (line_end - data->line_buf->str > pos && line_end[-1] == '\r')
        line_end[-1] = '\0';

      status = pdb_revo_list_files_process_line (data->line_buf->str + pos,
                                                 data,
                                                 error);
      if (status != PDB_REVO_READ_STATUS_OK)
        return status;

      pos = line_end - data->line_buf->str + 1;
    }

  /* Move any partial lines back to the beginning of the buffer */
  memmove (data->line_buf->str,
           data->line_buf->str + pos,
           data->line_buf->len - pos);
  g_string_set_size (data->line_buf, data->line_buf->len - pos);

  return PDB_REVO_READ_STATUS_OK;
}

char **
pdb_revo_list_files (PdbRevo *revo,
                     const char *glob,
                     GError **error)
{
  PdbRevoListFilesData data;
  gchar *argv[] = { "unzip", "-l", revo->zip_file, (char *) glob, NULL };
  char **ret;

  data.files = g_ptr_array_new ();
  data.line_buf = g_string_new (NULL);

  if (pdb_revo_execute_command (revo,
                                argv,
                                pdb_revo_list_files_cb,
                                &data,
                                error))
    {
      g_ptr_array_add (data.files, NULL);
      ret = (char **) g_ptr_array_free (data.files, FALSE);
    }
  else
    {
      int i;

      for (i = 0; i < data.files->len; i++)
        g_free (g_ptr_array_index (data.files, i));

      g_ptr_array_free (data.files, TRUE);

      ret = NULL;
    }

  g_string_free (data.line_buf, TRUE);

  return ret;
}

gboolean
pdb_revo_parse_file (PdbRevo *revo,
                     const char *filename,
                     PdbRevoReadCb func,
                     void *user_data,
                     GError **error)
{
  char *argv[] = { "unzip", "-p", revo->zip_file, (char *) filename, NULL };

  return pdb_revo_execute_command (revo,
                                   argv,
                                   func,
                                   user_data,
                                   error);
}

void
pdb_revo_free (PdbRevo *revo)
{
  g_free (revo->zip_file);
  g_slice_free (PdbRevo, revo);
}
