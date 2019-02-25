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
import android.content.ClipData;
import android.content.Context;
import android.content.ClipboardManager;
import android.text.Html;
import android.text.Spanned;

public abstract class SpannedCopy
{
  public static final String TAG = "prevoclip";

  public static void copyText (Context context,
                               CharSequence label,
                               CharSequence text)
  {
    ClipboardManager clipboard =
      (ClipboardManager) context.getSystemService (Context.CLIPBOARD_SERVICE);

    ClipData clipData;

    if (text instanceof Spanned)
      {
        String htmlText = Html.toHtml ((Spanned) text);

        clipData = ClipData.newHtmlText (label, text, htmlText);
      }
    else
      clipData = ClipData.newPlainText (label, text);

    clipboard.setPrimaryClip (clipData);
  }
}
