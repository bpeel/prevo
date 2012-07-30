/*
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
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

/* This code is based on nsBulletFrame.cpp from Mozilla */

#include "config.h"

#include <string.h>

#include "pdb-roman.h"

static const char pdb_roman_chars_a[] = "IXCM";
static const char pdb_roman_chars_b[] = "VLD";

void
pdb_roman_to_text_append (int ordinal,
                          GString *result)
{
  char *dec_str;
  const char *dp;
  int roman_pos, n;

  dec_str = g_strdup_printf ("%i", ordinal);

  if (ordinal < 1 || ordinal > 3999)
    {
      g_string_append (result, dec_str);
      g_free (dec_str);
      return;
    }

  roman_pos = strlen (dec_str);

  for (dp = dec_str; *dp; dp++)
    {
      roman_pos--;

      switch (*dp)
	{
	case '3':
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	  /*  FALLTHROUGH */
	case '2':
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	  /*  FALLTHROUGH */
	case '1':
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	  break;
	case '4':
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	  /*  FALLTHROUGH */
	case '5':
	case '6':
	case '7':
	case '8':
	  g_string_append_c (result, pdb_roman_chars_b[roman_pos]);
	  for (n = 0; '5' + n < *dp; n++)
	    {
	      g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	    }
	  break;
	case '9':
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos]);
	  g_string_append_c (result, pdb_roman_chars_a[roman_pos + 1]);
	  break;
	default:
	  break;
	}
    }

  g_free (dec_str);
}

char *
pdb_roman_to_text (int ordinal)
{
  GString *buf = g_string_new (NULL);

  pdb_roman_to_text_append (ordinal, buf);

  return g_string_free (buf, FALSE);
}
