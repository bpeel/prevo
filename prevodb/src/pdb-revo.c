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

struct _PdbRevoFile
{
  GPid child_pid;
  int child_out, child_err;
  gboolean in_end;
  gboolean err_end;
  gboolean child_reaped;
  GString *error_buf;
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

static PdbRevoFile *
pdb_revo_open_command (PdbRevo *revo,
                       char **argv,
                       GError **error)
{
  int child_out, child_err;
  GPid child_pid;
  PdbRevoFile *revo_file;

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
    return NULL;

  revo_file = g_slice_new (PdbRevoFile);

  revo_file->child_pid = child_pid;
  revo_file->child_out = child_out;
  revo_file->child_err = child_err;
  revo_file->in_end = FALSE;
  revo_file->err_end = FALSE;
  revo_file->child_reaped = FALSE;
  revo_file->error_buf = g_string_new (NULL);

  return revo_file;
}

static gboolean
pdb_revo_check_error (PdbRevoFile *file,
                      GError **error)
{
  int child_status;

  /* This probably shouldn't happen because that would mean we've
   * already reported an error or end-of-file. */
  if (file->child_reaped)
    return TRUE;

  while (waitpid (file->child_pid, &child_status, 0 /* options */) == -1 &&
         errno == EINTR);

  file->child_reaped = TRUE;

  if (!WIFEXITED (child_status) || WEXITSTATUS (child_status))
    {
      char *end = strchr (file->error_buf->str, '\n');

      if (end == NULL)
        end = file->error_buf->str + file->error_buf->len;
      else
        {
          while (end > file->error_buf->str && g_ascii_isspace (end[-1]))
            end--;
          *end = '\0';
        }

      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_UNZIP_FAILED,
                   "%s",
                   file->error_buf->str[0] ?
                   file->error_buf->str :
                   "Unzip failed");
      return FALSE;
    }

  return TRUE;
}

gboolean
pdb_revo_read (PdbRevoFile *file,
               char *buf,
               size_t *buflen,
               GError **error)
{
  size_t total_got = 0;

  while (*buflen > 0)
    {
      GPollFD fds[2];
      int nfds = 0;
      int i;

      if (!file->err_end)
        {
          fds[nfds].fd = file->child_err;
          fds[nfds].events = G_IO_IN | G_IO_ERR | G_IO_HUP;
          fds[nfds].revents = 0;
          nfds++;
        }

      if (!file->in_end)
        {
          fds[nfds].fd = file->child_out;
          fds[nfds].events = G_IO_IN | G_IO_ERR | G_IO_HUP;
          fds[nfds].revents = 0;
          nfds++;
        }

      if (nfds == 0)
        {
          *buflen = total_got;
          return pdb_revo_check_error (file, error);
        }

      if (g_poll (fds, nfds, -1) == -1)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "poll: %s",
                       strerror (errno));
          return FALSE;
        }

      for (i = 0; i < nfds; i++)
        if (fds[i].fd == file->child_out &&
            (fds[i].revents & (G_IO_IN | G_IO_ERR | G_IO_HUP)))
          {
            ssize_t got = read (file->child_out, buf, *buflen);

            if (got == -1)
              {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             "%s",
                             strerror (errno));
                return FALSE;
              }
            else if (got == 0)
              file->in_end = TRUE;
            else
              {
                total_got += got;
                *buflen -= got;
                buf += got;
              }
          }
        else if (fds[i].fd == file->child_err &&
                 (fds[i].revents & (G_IO_IN | G_IO_ERR | G_IO_HUP)))

          {
            ssize_t got;

            g_string_set_size (file->error_buf, file->error_buf->len + 512);

            got = read (file->child_err, file->error_buf->str, 512);

            if (got == -1)
              {
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             "%s",
                             strerror (errno));
                return FALSE;
              }
            else
              {
                g_string_set_size (file->error_buf,
                                   file->error_buf->len - 512 + got);
                if (got == 0)
                  file->err_end = TRUE;
              }
          }
    }

  *buflen = total_got;

  return TRUE;
}

void
pdb_revo_close (PdbRevoFile *file)
{
  int child_status;

  close (file->child_out);
  close (file->child_err);

  if (!file->child_reaped)
    while (waitpid (file->child_pid, &child_status, 0 /* options */) == -1 &&
           errno == EINTR);

  g_string_free (file->error_buf, TRUE);

  g_slice_free (PdbRevoFile, file);
}

typedef struct
{
  GString *line_buf;
  GPtrArray *files;
  gboolean in_list;
} PdbRevoListFilesData;

static gboolean
pdb_revo_list_files_process_line (PdbRevoListFilesData *data,
                                  const char *line,
                                  GError **error)
{
  if (g_str_has_prefix (line, "---"))
    {
      data->in_list = !data->in_list;
      return TRUE;
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

  return TRUE;

 error:
  g_set_error (error,
               PDB_ERROR,
               PDB_ERROR_UNZIP_FAILED,
               "Unexepected data from unzip");

  return FALSE;
}

static gboolean
pdb_revo_list_files_handle_data (PdbRevoListFilesData *data,
                                 const char *buf,
                                 int len,
                                 GError **error)
{
  char *line_end;
  int pos = 0;

  if (memchr (buf, '\0', len))
    {
      g_set_error (error, PDB_ERROR, PDB_ERROR_BAD_FORMAT,
                   "%s", "Embedded '\0' found in unzip listing");
      return FALSE;
    }

  g_string_append_len (data->line_buf, buf, len);

  while ((line_end = memchr (data->line_buf->str + pos,
                             '\n',
                             data->line_buf->len - pos)))
    {
      *line_end = '\0';

      if (line_end - data->line_buf->str > pos && line_end[-1] == '\r')
        line_end[-1] = '\0';

      if (!pdb_revo_list_files_process_line (data,
                                             data->line_buf->str + pos,
                                             error))
        return FALSE;

      pos = line_end - data->line_buf->str + 1;
    }

  /* Move any partial lines back to the beginning of the buffer */
  memmove (data->line_buf->str,
           data->line_buf->str + pos,
           data->line_buf->len - pos);
  g_string_set_size (data->line_buf, data->line_buf->len - pos);

  return TRUE;
}

char **
pdb_revo_list_files (PdbRevo *revo,
                     const char *glob,
                     GError **error)
{
  PdbRevoListFilesData data;
  PdbRevoFile *file;
  gchar *argv[] = { "unzip", "-l", revo->zip_file, (char *) glob, NULL };
  gboolean res = TRUE;

  data.files = g_ptr_array_new ();
  data.line_buf = g_string_new (NULL);

  if ((file = pdb_revo_open_command (revo, argv, error)))
    {
      char buf[512];
      size_t got;

      while (TRUE)
        {
          got = sizeof (buf);
          if (!pdb_revo_read (file, buf, &got, error) ||
              !pdb_revo_list_files_handle_data (&data, buf, got, error))
            {
              res = FALSE;
              break;
            }
          else if (got < sizeof (buf))
            break;
        }

      pdb_revo_close (file);
    }

  g_string_free (data.line_buf, TRUE);

  if (res)
    {
      g_ptr_array_add (data.files, NULL);
      return (char **) g_ptr_array_free (data.files, FALSE);
    }
  else
    {
      int i;

      for (i = 0; i < data.files->len; i++)
        g_free (g_ptr_array_index (data.files, i));

      g_ptr_array_free (data.files, TRUE);

      return NULL;
    }
}

static char *
pdb_revo_expand_filename (const char *filename)
{
  char **parts = g_strsplit (filename, "/", 0);
  char **parts_copy;
  char **src, **dst;
  char *ret;
  int n_parts;

  for (n_parts = 0; parts[n_parts]; n_parts++);

  parts_copy = g_alloca (sizeof (char *) * (n_parts + 1));

  for (src = parts, dst = parts_copy; *src; src++)
    {
      if (!strcmp (*src, ".."))
        {
          if (dst > parts_copy)
            dst--;
        }
      else if (strcmp (*src, "."))
        *(dst++) = *src;
    }

  *(dst++) = NULL;

  ret = g_strjoinv ("/", parts_copy);

  g_strfreev (parts);

  return ret;
}

PdbRevoFile *
pdb_revo_open (PdbRevo *revo,
               const char *filename,
               GError **error)
{
  char *argv[] = { "unzip", "-p", revo->zip_file, NULL, NULL };
  PdbRevoFile *file;

  argv[3] = pdb_revo_expand_filename (filename);

  file = pdb_revo_open_command (revo, argv, error);

  g_free (argv[3]);

  return file;
}

void
pdb_revo_free (PdbRevo *revo)
{
  g_free (revo->zip_file);
  g_slice_free (PdbRevo, revo);
}
