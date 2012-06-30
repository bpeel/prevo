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

package uk.co.busydoingnothing.prevo;

public class Hats
{
  public static String removeHats (CharSequence string)
  {
    StringBuilder result = new StringBuilder ();

    for (int i = 0; i < string.length (); i++)
      {
        if (i + 1 < string.length () &&
            (string.charAt (i + 1) == 'x' ||
             string.charAt (i + 1) == 'X'))
          {
            switch (string.charAt (i))
              {
              case 'c':
                result.append ('ĉ');
                break;
              case 'C':
                result.append ('Ĉ');
                break;
              case 'g':
                result.append ('ĝ');
                break;
              case 'G':
                result.append ('Ĝ');
                break;
              case 'h':
                result.append ('ĥ');
                break;
              case 'H':
                result.append ('Ĥ');
                break;
              case 'j':
                result.append ('ĵ');
                break;
              case 'J':
                result.append ('Ĵ');
                break;
              case 's':
                result.append ('ŝ');
                break;
              case 'S':
                result.append ('Ŝ');
                break;
              case 'u':
                result.append ('ŭ');
                break;
              case 'U':
                result.append ('Ŭ');
                break;
              default:
                result.append (string.subSequence (i, i + 2));
                break;
              }
            i++;
          }
        else
          result.append (string.charAt (i));
      }

    return result.toString ();
  }
}
