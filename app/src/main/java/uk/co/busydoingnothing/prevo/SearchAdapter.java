/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2012, 2016  Neil Roberts
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
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.TextView;
import java.util.Locale;

class SearchResultData
{
  public SearchResult[] results;
  public int count;
  public int languageNum;
}

public class SearchAdapter extends BaseAdapter
  implements Filterable
{
  static private final int MAX_RESULTS = 128;

  private Context context;
  private SearchFilter filter;

  private SearchResult noteResult;
  private SearchResult[] results;
  private int languageNum;
  private int numResults = 0;

  private String[] languages;

  public SearchAdapter (Context context,
                        String[] languages)
  {
    this.context = context;
    this.languages = languages;

    setResultData (doSearch (""));
  }

  private void setResultData (SearchResultData resultData)
  {
    results = resultData.results;
    languageNum = resultData.languageNum;
    numResults = resultData.count;

    if (languageNum > 0)
      {
        LanguageList languageList = LanguageList.getDefault (context);
        String mainLanguage =
          languageList.getLanguageName (languages[0],
                                        true /* with article */);
        String otherLanguage =
          languageList.getLanguageName (languages[languageNum],
                                        true /* with article */);
        Resources resources = context.getResources ();
        String note = resources.getString (R.string.other_language_note,
                                           mainLanguage,
                                           otherLanguage);
        noteResult = new SearchResult (note,
                                       Integer.MAX_VALUE,
                                       Integer.MAX_VALUE);
      }
  }

  private SearchResultData doSearch(String filterString)
  {
    SearchResultData resultData = new SearchResultData ();

    resultData.results = new SearchResult[MAX_RESULTS];

    try
      {
        for (int i = 0; i < languages.length; i++)
          {
            String language = languages[i];
            Trie trie = TrieCache.getTrie (context, language);
            int numResults = trie.search (filterString, resultData.results);

            if (numResults > 0)
              {
                resultData.count = numResults;
                resultData.languageNum = i;
                return resultData;
              }
          }

        resultData.languageNum = 0;
        resultData.count = 0;
        return resultData;
      }
    catch (java.io.IOException e)
      {
        throw new IllegalStateException ("Error while loading " +
                                         "an asset");
      }
  }

  @Override
  public int getCount ()
  {
    if (languageNum > 0)
      return numResults + 1;
    else
      return numResults;
  }

  @Override
  public SearchResult getItem (int position)
  {
    if (languageNum > 0)
      {
        if (position == 0)
          return noteResult;

        position--;
      }

    return results[position];
  }

  @Override
  public long getItemId (int position)
  {
    SearchResult result = getItem (position);
    return (result.getArticle () << 32L) | result.getMark ();
  }

  @Override
  public int getItemViewType (int position)
  {
    if (languageNum == 0 || position > 0)
      return 1;
    else
      return 0;
  }

  @Override
  public int getViewTypeCount ()
  {
    return 2;
  }

  @Override
  public View getView (int position, View convertView, ViewGroup parent)
  {
    TextView tv;

    if (convertView == null)
      {
        LayoutInflater layoutInflater = LayoutInflater.from (context);
        int id;
        if (languageNum == 0 || position > 0)
          id = android.R.layout.simple_list_item_1;
        else
          id = R.layout.search_note;
        tv = (TextView) layoutInflater.inflate (id, parent, false);
      }
    else
      tv = (TextView) convertView;

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
    return numResults == 0;
  }

  @Override
  public boolean isEnabled (int position)
  {
    /* If a language other than the primary one was used to return
     * results then the first item will be a note explaining it and it
     * shouldn't be enabled. */
    return languageNum == 0 || position > 0;
  }

  @Override
  public boolean areAllItemsEnabled ()
  {
    return false;
  }

  @Override
  public Filter getFilter ()
  {
    if (filter == null)
      filter = new SearchFilter ();

    return filter;
  }

  private static CharSequence trimCharSequence (CharSequence seq)
  {
    int length = seq.length ();

    int start;

    for (start = 0; start < length; start++)
      {
        if (!Character.isWhitespace (seq.charAt (start)))
          break;
      }

    int end;

    for (end = length; end > start; end--)
      {
        if (!Character.isWhitespace (seq.charAt (end - 1)))
          break;
      }

    return seq.subSequence (start, end);
  }

  private class SearchFilter extends Filter
  {
    @Override
    public FilterResults performFiltering (CharSequence filter)
    {
      FilterResults ret = new FilterResults ();
      CharSequence trimmedFilter = trimCharSequence (filter);
      String hatlessFilter = Hats.removeHats (trimmedFilter);
      String lowercaseFilter = hatlessFilter.toLowerCase (Locale.ROOT);
      SearchResultData resultData = doSearch (lowercaseFilter);

      ret.count = resultData.count;
      ret.values = resultData;

      return ret;
    }

    @Override
    public void publishResults (CharSequence filter, FilterResults res)
    {
      setResultData ((SearchResultData) res.values);

      if (res.count > 0)
        notifyDataSetChanged();
      else
        notifyDataSetInvalidated();
    }
  }
}
