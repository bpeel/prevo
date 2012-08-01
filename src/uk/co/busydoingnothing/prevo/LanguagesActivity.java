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

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.ListActivity;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.URLSpan;
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

  private static final int DIALOG_ABOUT = 0;

  private static final String LICENSE_URL =
    "http://www.gnu.org/licenses/gpl-2.0-standalone.html";
  private static final String RETA_VORTARO_URL =
    "http://purl.org/net/voko/revo/";

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
  public boolean onCreateOptionsMenu(Menu menu)
  {
    MenuInflater inflater = getMenuInflater ();
    inflater.inflate (R.menu.main_menu, menu);

    return super.onCreateOptionsMenu (menu);
  }

  @Override
  public boolean onOptionsItemSelected (MenuItem item)
  {
    switch (item.getItemId ())
      {
      case R.id.menu_about:
        showDialog (DIALOG_ABOUT);
        break;
      }

    return super.onOptionsItemSelected (item);
  }

  private void linkifyAboutMessage (SpannableString string)
  {
    int pos;

    if ((pos = string.toString ().indexOf ("Click here for")) != -1)
      {
        URLSpan span = new URLSpan (LICENSE_URL);
        string.setSpan (span, pos + 6, pos + 10, 0 /* flags */);
      }

    if ((pos = string.toString ().indexOf ("Reta Vortaro")) != -1)
      {
        URLSpan span = new URLSpan (RETA_VORTARO_URL);
        string.setSpan (span, pos, pos + 12, 0 /* flags */);
      }
  }

  @Override
  protected Dialog onCreateDialog (int id)
  {
    Dialog dialog;
    Resources res = getResources ();

    switch (id)
      {
      case DIALOG_ABOUT:
        {
          AlertDialog.Builder builder = new AlertDialog.Builder (this);
          CharSequence message = res.getText (R.string.about_message);

          if (message instanceof Spanned)
            {
              message = new SpannableString (message);
              linkifyAboutMessage ((SpannableString) message);
            }

          LayoutInflater layoutInflater = getLayoutInflater ();
          TextView tv =
            (TextView) layoutInflater.inflate (R.layout.about_view,
                                               null);
          tv.setText (message);
          tv.setMovementMethod (LinkMovementMethod.getInstance ());

          builder
            .setView (tv)
            .setCancelable (true)
            .setNegativeButton (R.string.close,
                                new DialogInterface.OnClickListener ()
                                {
                                  @Override
                                  public void onClick (DialogInterface dialog,
                                                       int whichButton)
                                  {
                                  }
                                });
          dialog = builder.create ();
        }
        break;

      default:
        dialog = null;
        break;
      }

    return dialog;
  }
}
