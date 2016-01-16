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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.text.method.LinkMovementMethod;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.URLSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

public class MenuHelper
{
  private static final String LICENSE_URL =
    "http://www.gnu.org/licenses/gpl-2.0-standalone.html";
  private static final String RETA_VORTARO_URL =
    "http://purl.org/net/voko/revo/";
  public static final String PREVO_PREFERENCES =
    "PrevoPreferences";
  public static final String PREF_LAST_LANGUAGE =
    "lastLanguage";
  public static final String PREF_FONT_SIZE =
    "fontSize";

  private static final int DIALOG_ABOUT = 0;

  private static void linkifyAboutMessage (Context context,
                                           SpannableStringBuilder string)
  {
    int pos;

    if ((pos = string.toString ().indexOf ("@VERSION@")) != -1)
      {
        String packageVersion;

        try
          {
            PackageManager manager = context.getPackageManager ();
            String packageName = context.getPackageName ();
            PackageInfo packageInfo = manager.getPackageInfo (packageName, 0);

            packageVersion = packageInfo.versionName;
          }
        catch (PackageManager.NameNotFoundException e)
          {
            packageVersion = "?";
          }

        string.replace (pos, pos + 9, packageVersion);
      }

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

  public static Dialog onCreateDialog (Activity activity,
                                       int id)
  {
    Dialog dialog;
    Resources res = activity.getResources ();

    switch (id)
      {
      case DIALOG_ABOUT:
        {
          AlertDialog.Builder builder = new AlertDialog.Builder (activity);
          SpannableStringBuilder message =
            new SpannableStringBuilder (res.getText (R.string.about_message));

          linkifyAboutMessage (activity, message);

          LayoutInflater layoutInflater = activity.getLayoutInflater ();
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

  public static Intent createSearchIntent (Activity activity,
                                           String language)
  {
    Intent intent = new Intent (activity, SearchActivity.class);

    intent.putExtra (SearchActivity.EXTRA_LANGUAGE, language);

    return intent;
  }

  public static void goSearch (Activity activity)
  {
    SharedPreferences prefs =
      activity.getSharedPreferences (PREVO_PREFERENCES,
                                     Activity.MODE_PRIVATE);
    String defaultLanguage = prefs.getString (PREF_LAST_LANGUAGE, "eo");
    Intent intent = createSearchIntent (activity, defaultLanguage);

    activity.startActivity (intent);
  }

  public static void goChooseLanguage (Activity activity)
  {
    Intent intent = new Intent (activity, SelectLanguageActivity.class);
    activity.startActivity (intent);
  }

  public static void showAbout (Activity activity)
  {
    activity.showDialog (DIALOG_ABOUT);
  }

  public static void goPreferences (Activity activity)
  {
    Intent intent = new Intent (activity, PreferenceActivity.class);
    activity.startActivity (intent);
  }

  public static boolean onOptionsItemSelected (Activity activity,
                                               MenuItem item)
  {
    switch (item.getItemId ())
      {
      case R.id.menu_choose_language:
        goChooseLanguage (activity);
        return true;

      case R.id.menu_search:
        goSearch (activity);
        return true;

      case R.id.menu_preferences:
        goPreferences (activity);
        return true;

      case R.id.menu_about:
        showAbout (activity);
        return true;
      }

    return false;
  }
}
