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
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.TextView;
import java.util.Vector;

public class LanguagesAdapter extends BaseAdapter
  implements Filterable
{
  private Language[] mainLanguages =
  {
    new Language ("esperanto", "eo")
  };

  private LanguageList languageList;
  private Language[] selectedLanguages;

  private Context context;
  private LanguagesFilter filter;
  private Language[] filteredLanguages;

  static final int TYPE_HEADER = 0;
  static final int TYPE_LANGUAGE = 1;
  static final int TYPE_COUNT = 2;

  public LanguagesAdapter (Context context)
  {
    this.context = context;
    this.languageList = LanguageList.getDefault (context);
    this.selectedLanguages = getSelectedLanguages ();
  }

  private Language[] getSelectedLanguages ()
  {
    SelectedLanguages selectedLanguages = new SelectedLanguages (context);
    Language[] allLanguages = languageList.getAllLanguages ();

    if (selectedLanguages.containsAll ())
      return allLanguages;

    Vector<Language> languages = new Vector<Language> ();

    for (int i = 0; i < allLanguages.length; i++)
      if (selectedLanguages.contains (allLanguages[i].getCode ()))
        languages.add (allLanguages[i]);

    return languages.toArray (new Language[languages.size ()]);
  }

  @Override
  public int getCount ()
  {
    if (filteredLanguages == null)
      return mainLanguages.length + selectedLanguages.length + 2;
    else
      return filteredLanguages.length + 1;
  }

  @Override
  public Object getItem (int position)
  {
    if (filteredLanguages == null)
      {
        if (position == 0)
          return "ĈEFAJ LINGVOJ";

        position--;

        if (position < mainLanguages.length)
          return mainLanguages[position];

        position -= mainLanguages.length;

        if (position == 0)
          return "ĈIUJ LINGVOJ";

        return selectedLanguages[position - 1];
      }
    else if (position == 0)
      return "ĈIUJ LINGVOJ";
    else
      return filteredLanguages[position - 1];
  }

  @Override
  public long getItemId (int position)
  {
    return position;
  }

  @Override
  public int getItemViewType (int position)
  {
    if (filteredLanguages == null)
      {
        if (position == 0 || position == mainLanguages.length + 1)
          return TYPE_HEADER;
        else
          return TYPE_LANGUAGE;
      }
    else if (position == 0)
      return TYPE_HEADER;
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

          case TYPE_LANGUAGE:
            id = android.R.layout.simple_list_item_1;
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
    return false;
  }

  @Override
  public boolean isEmpty ()
  {
    return false;
  }

  @Override
  public boolean isEnabled (int position)
  {
    return getItemViewType (position) == TYPE_LANGUAGE;
  }

  @Override
  public boolean areAllItemsEnabled ()
  {
    return false;
  }

  @Override
  public LanguagesFilter getFilter ()
  {
    if (filter == null)
      filter = new LanguagesFilter (selectedLanguages);

    return filter;
  }

  public void setMainLanguages (String[] languages)
  {
    Language[] mainLanguages = new Language[languages.length + 1];

    /* Preserve the 'esperanto' language */
    mainLanguages[0] = this.mainLanguages[0];

    for (int i = 0; i < languages.length; i++)
      {
        String languageName = languageList.getLanguageName (languages[i]);
        mainLanguages[i + 1] = new Language (languageName, languages[i]);
      }

    this.mainLanguages = mainLanguages;

    notifyDataSetChanged ();
  }

  public void reload ()
  {
    this.selectedLanguages = getSelectedLanguages ();
    notifyDataSetChanged ();
  }

  private class LanguagesFilter extends Filter
  {
    private Language[] selectedLanguages;

    public LanguagesFilter (Language[] selectedLanguages)
    {
      /* We keep a copy of the languages array so that we don't have
       * to worry about it being replaced in the main thread */
      this.selectedLanguages = selectedLanguages;
    }

    @Override
    public FilterResults performFiltering (CharSequence filter)
    {
      FilterResults ret = new FilterResults ();

      if (filter.length () == 0)
        {
          ret.values = null;
          ret.count = 0;
        }
      else
        {
          String filterString = Hats.removeHats (filter);
          Vector<Language> result = new Vector<Language> ();

          for (int i = 0; i < selectedLanguages.length; i++)
            {
              Language language = selectedLanguages[i];

              if (language.getName ().startsWith (filterString))
                result.add (language);
            }


          ret.values = result.toArray (new Language[result.size ()]);
          ret.count = result.size ();
        }

      return ret;
    }

    @Override
    public void publishResults (CharSequence filter, FilterResults results)
    {
      filteredLanguages = (Language[]) results.values;
      notifyDataSetChanged ();
    }
  }
}
