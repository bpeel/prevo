/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2013  Neil Roberts
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

import android.support.v7.app.AppCompatActivity;
import android.content.Context;
import android.content.SharedPreferences;
import java.util.Arrays;

public class SelectedLanguages
{
  public static final String PREF = "selectedLanguages";

  private String[] languages;

  public SelectedLanguages (Context context)
  {
    SharedPreferences prefs =
      context.getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                                    AppCompatActivity.MODE_PRIVATE);
    String languagesString = prefs.getString (PREF, null);

    if (languagesString != null)
      {
        int length = languagesString.length ();

        if (length == 0)
          languages = new String[0];
        else
          {
            int commas = 0;
            int i;

            for (i = 0; i < length; i++)
              if (languagesString.charAt (i) == ',')
                commas++;

            languages = new String[commas + 1];
            int lastPos = 0;

            for (i = 0; i < commas; i++)
              {
                int commaPos = languagesString.indexOf (',', lastPos);
                languages[i] = languagesString.substring (lastPos, commaPos);
                lastPos = commaPos + 1;
              }

            languages[commas] = languagesString.substring (lastPos);

            Arrays.sort (languages);
          }
      }
  }

  public boolean contains (String language)
  {
    if (containsAll ())
      return true;

    /* Esperanto is always included */
    if (language.equals ("eo"))
      return true;

    return Arrays.binarySearch (languages, language) >= 0;
  }

  public boolean containsAll ()
  {
    /* null is used to indicate that we want all languages */
    return languages == null;
  }
}
