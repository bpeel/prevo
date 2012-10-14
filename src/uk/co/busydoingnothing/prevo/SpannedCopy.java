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

import android.util.Log;
import android.content.Context;
import android.text.ClipboardManager;
import android.text.Html;
import android.text.Spanned;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public abstract class SpannedCopy
{
  private static boolean initialised = false;

  private static boolean supported = false;

  private static Class clipDataClass;
  private static Class clipboardManagerClass;
  private static Method newPlainTextMethod;
  private static Method newHtmlTextMethod;
  private static Method setPrimaryClipMethod;

  public static final String TAG = "prevoclip";

  private static void initialise ()
  {
    if (!initialised)
      {
        try
          {
            clipDataClass = Class.forName ("android.content.ClipData");
            newPlainTextMethod =
              clipDataClass.getMethod ("newPlainText",
                                       CharSequence.class,
                                       CharSequence.class);
            newHtmlTextMethod =
              clipDataClass.getMethod ("newHtmlText",
                                       CharSequence.class,
                                       CharSequence.class,
                                       String.class);
            clipboardManagerClass =
              Class.forName ("android.content.ClipboardManager");
            setPrimaryClipMethod =
              clipboardManagerClass.getMethod ("setPrimaryClip",
                                               clipDataClass);

            supported = true;
          }
        catch (ClassNotFoundException e)
          {
            Log.i (TAG, "Clipboard not supported: " + e.getMessage ());
          }
        catch (NoSuchMethodException e)
          {
            Log.i (TAG, "Clipboard not supported: " + e.getMessage ());
          }

        initialised = true;
      }
  }

  public static void copyText (Context context,
                               CharSequence label,
                               CharSequence text)
  {
    initialise ();

    ClipboardManager clipboard =
      (ClipboardManager) context.getSystemService (Context.CLIPBOARD_SERVICE);

    if (supported)
      {
        Object clipData;

        try
          {
            if (text instanceof Spanned)
              {
                String htmlText = Html.toHtml ((Spanned) text);

                clipData = newHtmlTextMethod.invoke (null, /* receiver */
                                                     label,
                                                     text,
                                                     htmlText);
              }
            else
              clipData = newPlainTextMethod.invoke (null, /* receiver */
                                                    label,
                                                    text);

            setPrimaryClipMethod.invoke (clipboard, clipData);
          }
        catch (IllegalAccessException e)
          {
          }
        catch (InvocationTargetException e)
          {
          }
      }
    else
      clipboard.setText (text);
  }
}
