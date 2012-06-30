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

#include "pdb-strcmp.h"

static guint32
pdb_strcmp_get_value_ch (gunichar ch)
{
  ch = g_unichar_tolower (ch);

  switch (ch)
    {
    case 0x109:
      return 'c' * 2U + 1;
    case 0x11d:
      return 'g' * 2U + 1;
    case 0x125:
      return 'h' * 2U + 1;
    case 0x135:
      return 'j' * 2U + 1;
    case 0x15d:
      return 's' * 2U + 1;
    case 0x16d:
      return 'u' * 2U + 1;
    default:
      return ch * 2U;
    }
}

static guint32
pdb_strcmp_get_value (const char *p)
{
  return pdb_strcmp_get_value_ch (g_utf8_get_char (p));
}

int
pdb_strcmp_ch (gunichar a,
               gunichar b)
{
  guint32 av = pdb_strcmp_get_value_ch (a);
  guint32 bv = pdb_strcmp_get_value_ch (b);

  if (av < bv)
    return -1;
  else if (av > bv)
    return 1;
  else
    return 0;
}

/* Compares two strings using Esperanto orthography */
int
pdb_strcmp (const char *a,
            const char *b)
{
  while (TRUE)
    {
      guint32 av = pdb_strcmp_get_value (a);
      guint32 bv = pdb_strcmp_get_value (b);

      if (av < bv)
        return -1;
      else if (av > bv)
        return 1;
      else if (av == '\0')
        return 0;
      else
        {
          a = g_utf8_next_char (a);
          b = g_utf8_next_char (b);
        }
    }
}
