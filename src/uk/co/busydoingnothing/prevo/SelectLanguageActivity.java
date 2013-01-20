/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2012, 2013  Neil Roberts
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

import android.app.Activity;
import android.app.ListActivity;
import android.app.Dialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

public class SelectLanguageActivity extends ListActivity
  implements SharedPreferences.OnSharedPreferenceChangeListener
{
  private LanguageDatabaseHelper dbHelper;
  private LanguagesAdapter adapter;

  private boolean stopped;
  private boolean reloadQueued;

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setTitle (R.string.select_language);
    setContentView (R.layout.languages);

    adapter = new LanguagesAdapter (this);
    setListAdapter (adapter);

    dbHelper = new LanguageDatabaseHelper (this);

    stopped = true;
    reloadQueued = true;

    ListView lv = getListView ();

    lv.setTextFilterEnabled (true);

    lv.setOnItemClickListener (new AdapterView.OnItemClickListener ()
      {
        public void onItemClick (AdapterView<?> parent,
                                 View view,
                                 int position,
                                 long id)
        {
          LanguagesAdapter adapter =
            (LanguagesAdapter) parent.getAdapter ();
          Object item = adapter.getItem (position);

          if (item instanceof Language)
            {
              Language lang = (Language) item;
              Intent intent = new Intent (view.getContext (),
                                          SearchActivity.class);
              intent.putExtra (SearchActivity.EXTRA_LANGUAGE,
                               lang.getCode ());

              dbHelper.useLanguage (lang.getCode ());

              SharedPreferences prefs =
                getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                                      MODE_PRIVATE);
              SharedPreferences.Editor editor = prefs.edit ();
              editor.putString (MenuHelper.PREF_LAST_LANGUAGE, lang.getCode ());
              editor.commit ();

              startActivity (intent);
            }
        }
      });

    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            Activity.MODE_PRIVATE);
    prefs.registerOnSharedPreferenceChangeListener (this);
  }

  @Override
  public void onStart ()
  {
    super.onStart ();

    stopped = false;

    adapter.setMainLanguages (dbHelper.getLanguages ());

    if (reloadQueued)
      {
        adapter.reload ();
        reloadQueued = false;
      }
  }

  @Override
  public void onStop ()
  {
    stopped = true;

    super.onStop ();
  }

  @Override
  public void onDestroy ()
  {
    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            Activity.MODE_PRIVATE);

    prefs.unregisterOnSharedPreferenceChangeListener (this);

    super.onDestroy ();
  }

  @Override
  public boolean onCreateOptionsMenu (Menu menu)
  {
    MenuInflater inflater = getMenuInflater ();

    inflater.inflate (R.menu.main_menu, menu);

    return true;
  }

  @Override
  public boolean onOptionsItemSelected (MenuItem item)
  {
    if (MenuHelper.onOptionsItemSelected (this, item))
      return true;

    return super.onOptionsItemSelected (item);
  }

  @Override
  protected Dialog onCreateDialog (int id)
  {
    return MenuHelper.onCreateDialog (this, id);
  }

  @Override
  public void onSharedPreferenceChanged (SharedPreferences prefs,
                                         String key)
  {
    if (key.equals (SelectedLanguages.PREF))
      {
        if (stopped)
          /* Queue the reload for the next time the activity is started */
          reloadQueued = true;
        else
          {
            adapter.reload ();
            reloadQueued = false;
          }
      }
  }
}
