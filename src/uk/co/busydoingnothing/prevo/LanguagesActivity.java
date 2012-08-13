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

public class LanguagesActivity extends ListActivity
{
  private LanguageDatabaseHelper dbHelper;
  private LanguagesAdapter adapter;

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setTitle (R.string.select_language);
    setContentView (R.layout.languages);

    adapter = new LanguagesAdapter (this);
    setListAdapter (adapter);

    dbHelper = new LanguageDatabaseHelper (this);

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
  }

  @Override
  public void onStart ()
  {
    super.onStart ();

    adapter.setMainLanguages (dbHelper.getLanguages ());
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
}
