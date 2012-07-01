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

#include <stdio.h>
#include <glib.h>

static gboolean
dump_trie (const guint8 *trie_data,
           int trie_length,
           GString *buf)
{
  gunichar ch;
  int old_len;
  const guint8 *ch_end;
  guint32 offset;
  gboolean is_word;

  old_len = buf->len;

  if (trie_length < 4)
    {
      fprintf (stderr, "Unexpected end of trie\n");
      return FALSE;
    }

  offset = (((guint32) trie_data[0]) |
            (((guint32) trie_data[1]) << 8) |
            (((guint32) trie_data[2]) << 16) |
            (((guint32) trie_data[3]) << 24));

  is_word = !!(offset & (1 << (guint32) 31));
  offset &= ~(offset & (1 << (guint32) 31));

  if (offset != trie_length)
    {
      fprintf (stderr, "Offset does not equal trie node size\n");
      return FALSE;
    }

  trie_data += 4;
  trie_length -= 4;

  ch = g_utf8_get_char_validated ((const char *) trie_data, trie_length);

  if (ch >= (gunichar) -2)
    {
      fprintf (stderr, "Invalid unicode character encountered\n");
      return FALSE;
    }

  ch_end = (const guint8 *) g_utf8_next_char ((const char *) trie_data);

  g_string_append_len (buf, (const char *) trie_data, ch_end - trie_data);

  trie_length -= ch_end - trie_data;
  trie_data = ch_end;

  while (is_word)
    {
      int article_num, mark_num;
      gboolean has_display_name;

      if (trie_length < 3)
        {
          fprintf (stderr, "Unexpected end of trie\n");
          return FALSE;
        }

      article_num = trie_data[0] | ((int) trie_data[1] << 8);
      mark_num = trie_data[2];

      is_word = !!(article_num & 0x8000);
      has_display_name = !!(article_num & 0x4000);

      article_num &= 0x3fff;

      trie_data += 3;
      trie_length -= 3;

      printf ("%s ", buf->str + 1);

      if (has_display_name)
        {
          int len;

          if (trie_length < 1 ||
              trie_length < 1 + (len = trie_data[0]))
            {
              fprintf (stderr, "Unexpected end of trie\n");
              return FALSE;
            }

          fputc ('(', stdout);
          fwrite (trie_data + 1, 1, len, stdout);
          fputs (") ", stdout);

          trie_length -= len + 1;
          trie_data += len + 1;
        }

      printf ("%i %i\n", article_num, mark_num);

      if (is_word)
        printf ("+ ");
    }

  while (trie_length > 0)
    {
      offset = (((guint32) trie_data[0]) |
                (((guint32) trie_data[1]) << 8) |
                (((guint32) trie_data[2]) << 16) |
                (((guint32) trie_data[3]) << 24));

      is_word = !!(offset & (1 << (guint32) 31));
      offset &= ~(offset & (1 << (guint32) 31));

      if (offset > trie_length)
        {
          fprintf (stderr, "Child node is too big\n");
          return FALSE;
        }

      if (!dump_trie (trie_data, offset, buf))
        return FALSE;

      trie_length -= offset;
      trie_data += offset;
    }

  g_string_set_size (buf, old_len);

  return TRUE;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  int ret = 0;
  int i;

  for (i = 1; i < argc; i++)
    {
      const char *filename = argv[i];
      char *trie_data;
      gsize trie_length;

      if (g_file_get_contents (filename,
                               &trie_data,
                               &trie_length,
                               &error))
        {
          gboolean dump_ret;
          GString *buf;

          buf = g_string_new (NULL);

          dump_ret = dump_trie ((const guint8 *) trie_data, trie_length, buf);

          g_string_free (buf, TRUE);

          if (!dump_ret)
            {
              ret = 1;
              break;
            }
        }
      else
        {
          fprintf (stderr, "%s: %s\n", filename, error->message);
          g_clear_error (&error);
          ret = 1;
          break;
        }
    }

  return ret;
}
