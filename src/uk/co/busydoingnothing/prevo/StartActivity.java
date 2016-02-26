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
import android.content.SharedPreferences;
import android.os.Bundle;

/* This activity is just like a landing page to select the right
 * initial activity. If the user has already previously selected a
 * language then we'll default to searching in that language again,
 * otherwise we show the language select page */

public class StartActivity extends AppCompatActivity
{
  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    String lastLanguage;

    super.onCreate (savedInstanceState);

    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            AppCompatActivity.MODE_PRIVATE);

    lastLanguage = prefs.getString (MenuHelper.PREF_LAST_LANGUAGE, null);

    if (lastLanguage == null)
      MenuHelper.goChooseLanguage (this);
    else
      MenuHelper.goSearch (this);

    /* Finish this activity to get it out of the call stack */
    finish ();
  }
}
