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

import android.content.Context;
import android.content.SharedPreferences;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

public class PreferenceAdapter extends BaseAdapter
{
  private Language[] allLanguages;

  private Context context;

  private static final int TYPE_HEADER = 0;
  private static final int TYPE_LANGUAGE = 1;
  private static final int TYPE_ACTION = 2;
  private static final int TYPE_COUNT = 3;

  public static final int FIRST_LANGUAGE_POSITION = 3;

  public PreferenceAdapter (Context context)
  {
    this.context = context;

    LanguageList languageList = LanguageList.getDefault (context);

    /* Grab a list of all languages except Esperanto because that
     * isn't a translation */
    Language[] allLanguages = languageList.getAllLanguages ();
    this.allLanguages = new Language[allLanguages.length - 1];

    for (int src = 0, dst = 0; src < allLanguages.length; src++)
      if (!allLanguages[src].getCode ().equals ("eo"))
        this.allLanguages[dst++] = allLanguages[src];
  }

  @Override
  public int getCount ()
  {
    return allLanguages.length + FIRST_LANGUAGE_POSITION;
  }

  @Override
  public Object getItem (int position)
  {
    if (position == 0)
      return context.getString (R.string.show_translations);
    else if (position == 1)
      return context.getString (R.string.show_all_translations);
    else if (position == 2)
      return context.getString (R.string.show_no_translations);
    else
      return allLanguages[position - FIRST_LANGUAGE_POSITION];
  }

  @Override
  public long getItemId (int position)
  {
    return position;
  }

  @Override
  public int getItemViewType (int position)
  {
    if (position == 0)
      return TYPE_HEADER;
    else if (position == 1 || position == 2)
      return TYPE_ACTION;
    else
      return TYPE_LANGUAGE;
  }

  @Override
  public int getViewTypeCount ()
  {
    return TYPE_COUNT;
  }

  @Override
  public View getView (int position, View convertView, ViewGroup parent)
  {
    TextView tv;

    if (convertView == null)
      {
        LayoutInflater layoutInflater = LayoutInflater.from (context);
        int id;

        switch (getItemViewType (position))
          {
          case TYPE_HEADER:
            id = android.R.layout.preference_category;
            break;

          case TYPE_ACTION:
            id = android.R.layout.simple_list_item_1;
            break;

          case TYPE_LANGUAGE:
            id = android.R.layout.simple_list_item_multiple_choice;
            break;

          default:
            throw new IllegalStateException ();
          }

        tv = (TextView) layoutInflater.inflate (id, parent, false);
      }
    else
      {
        tv = (TextView) convertView;
      }

    tv.setText (getItem (position).toString ());

    return tv;
  }

  @Override
  public boolean hasStableIds ()
  {
    return true;
  }

  @Override
  public boolean isEmpty ()
  {
    return false;
  }

  @Override
  public boolean isEnabled (int position)
  {
    return getItemViewType (position) != TYPE_HEADER;
  }

  @Override
  public boolean areAllItemsEnabled ()
  {
    return false;
  }
}
