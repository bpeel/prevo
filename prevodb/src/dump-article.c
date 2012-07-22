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
dump_article (const guint8 *article_data,
              int article_length)
{
  while (article_length > 0)
    {
      int text_length;

      if (article_length < 2)
        {
          fprintf (stderr, "Invalid article length");
          return FALSE;
        }

      text_length = article_data[0] + (article_data[1] << 8);

      if (text_length + 2 > article_length)
        {
          fprintf (stderr,
                   "not enough data for string of length %i\n",
                   text_length);
          return FALSE;
        }

      fwrite (article_data + 2, 1, text_length, stdout);
      fputc ('\n', stdout);

      article_data += 2 + text_length;
      article_length -= 2 + text_length;

      while (TRUE)
        {
          int span_start, span_length, data1, data2, type;

          if (article_length < 2)
            {
              fprintf (stderr, "no space for span length");
              return FALSE;
            }

          span_length = article_data[0] + (article_data[1] << 8);

          article_data += 2;
          article_length -= 2;

          if (span_length == 0)
            break;

          if (article_length < 7)
            {
              fprintf (stderr, "no space for span length");
              return FALSE;
            }

          span_start = article_data[0] + (article_data[1] << 8);
          data1 = article_data[2] + (article_data[3] << 8);
          data2 = article_data[4] + (article_data[5] << 8);
          type = article_data[6];

          printf ("%i(%i) [%i,%i] %i\n",
                  span_start,
                  span_length,
                  data1,
                  data2,
                  type);

          article_data += 7;
          article_length -= 7;
        }
    }

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
      char *article_data;
      gsize article_length;

      if (g_file_get_contents (filename,
                               &article_data,
                               &article_length,
                               &error))
        {
          gboolean dump_ret;

          dump_ret = dump_article ((const guint8 *) article_data,
                                   article_length);

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
