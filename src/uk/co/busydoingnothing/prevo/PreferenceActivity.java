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
import android.app.ListActivity;
import android.app.Dialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.Looper;
import android.os.MessageQueue.IdleHandler;
import android.text.method.LinkMovementMethod;
import android.util.SparseBooleanArray;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

public class PreferenceActivity extends ListActivity
{
  private SharedPreferences prefs;
  private PreferenceAdapter adapter;
  private boolean queuedUpdate;

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setContentView (R.layout.preferences);

    adapter = new PreferenceAdapter (this);
    setListAdapter (adapter);

    prefs = getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                                  MODE_PRIVATE);

    ListView listView = getListView ();

    listView.setItemsCanFocus (false);
    listView.setChoiceMode (ListView.CHOICE_MODE_MULTIPLE);

    listView.setOnItemClickListener (new AdapterView.OnItemClickListener ()
      {
        public void onItemClick (AdapterView<?> parent,
                                 View view,
                                 int position,
                                 long id)
        {
          if (position == 1)
            setAll (true);
          else if (position == 2)
            setAll (false);

          /* Update the preference on idle so that the system will have time
           * to update the checked state after the click */
          queueUpdatePreference ();
        }
      });

    updateCheckedState ();
  }

  private void setAll (boolean value)
  {
    ListView listView = getListView ();
    int numPrefs = adapter.getCount ();

    for (int i = PreferenceAdapter.FIRST_LANGUAGE_POSITION; i < numPrefs; i++)
      listView.setItemChecked (i, value);
  }

  private void updateCheckedState ()
  {
    ListView listView = getListView ();
    SelectedLanguages selectedLanguages = new SelectedLanguages (this);
    int numPrefs = adapter.getCount ();

    for (int i = PreferenceAdapter.FIRST_LANGUAGE_POSITION; i < numPrefs; i++)
      {
        String language = ((Language) adapter.getItem (i)).getCode ();
        listView.setItemChecked (i, selectedLanguages.contains (language));
      }
  }

  private void updatePreference ()
  {
    ListView listView = getListView ();
    SparseBooleanArray checkedItems = listView.getCheckedItemPositions ();
    int totalLanguages =
      listView.getCount () - PreferenceAdapter.FIRST_LANGUAGE_POSITION;
    int numCheckedLanguages = 0;
    String value;

    /* Count the total number of set languages. If it's all or none of
     * the languages then we'll use a special value for the
     * preference */
    for (int i = 0; i < checkedItems.size (); i++)
      if (checkedItems.keyAt (i) >= PreferenceAdapter.FIRST_LANGUAGE_POSITION &&
          checkedItems.valueAt (i))
        numCheckedLanguages++;

    if (numCheckedLanguages == 0)
      value = "";
    else if (numCheckedLanguages >= totalLanguages)
      value = null;
    else
      {
        StringBuilder buf = new StringBuilder ();
        int checkedItemsSize = checkedItems.size ();

        for (int i = 0; i < checkedItemsSize; i++)
          {
            if (checkedItems.valueAt (i))
              {
                int key = checkedItems.keyAt (i);

                if (key >= PreferenceAdapter.FIRST_LANGUAGE_POSITION)
                  {
                    Language lang = (Language) adapter.getItem (key);

                    if (buf.length () > 0)
                      buf.append (',');

                    buf.append (lang.getCode ());
                  }
              }
          }

        value = buf.toString ();
      }

    SharedPreferences.Editor editor = prefs.edit ();
    if (value == null)
      editor.remove (SelectedLanguages.PREF);
    else
      editor.putString (SelectedLanguages.PREF, value);

    editor.commit ();
  }

  private void queueUpdatePreference ()
  {
    if (!queuedUpdate)
      {
        Looper.myQueue ().addIdleHandler (new IdleHandler ()
          {
            public boolean queueIdle ()
            {
              updatePreference ();
              queuedUpdate = false;
              /* Remove the handler */
              return false;
            }
          });

        queuedUpdate = true;
      }
  }
}
