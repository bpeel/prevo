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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.TextView;

public class SearchAdapter extends BaseAdapter
  implements Filterable
{
  static private final int MAX_RESULTS = 128;

  private Context context;
  private SearchFilter filter;

  private SearchResult[] results;
  private int numResults = 0;

  private String[] languages;

  public SearchAdapter (Context context,
                        String[] languages)
  {
    this.context = context;
    this.languages = languages;

    results = new SearchResult[MAX_RESULTS];
    numResults = doSearch ("", results);
  }

  private int doSearch(String filterString,
                       SearchResult[] results)
  {
    try
      {
        for (String language : languages)
          {
            Trie trie = TrieCache.getTrie (context, language);
            int numResults = trie.search (filterString, results);

            if (numResults > 0)
              return numResults;
          }

        return 0;
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
    return numResults;
  }

  @Override
  public SearchResult getItem (int position)
  {
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
    return 0;
  }

  @Override
  public int getViewTypeCount ()
  {
    return 1;
  }

  @Override
  public View getView (int position, View convertView, ViewGroup parent)
  {
    TextView tv;

    if (convertView == null)
      {
        LayoutInflater layoutInflater = LayoutInflater.from (context);
        int id = android.R.layout.simple_list_item_1;
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
    return true;
  }

  @Override
  public boolean areAllItemsEnabled ()
  {
    return true;
  }

  @Override
  public Filter getFilter ()
  {
    if (filter == null)
      filter = new SearchFilter ();

    return filter;
  }

  private class SearchFilter extends Filter
  {
    @Override
    public FilterResults performFiltering (CharSequence filter)
    {
      FilterResults ret = new FilterResults ();
      String filterString = Hats.removeHats (filter).toLowerCase ();
      SearchResult[] results = new SearchResult[MAX_RESULTS];

      ret.values = results;
      ret.count = doSearch (filterString, results);

      return ret;
    }

    @Override
    public void publishResults (CharSequence filter, FilterResults res)
    {
      results = (SearchResult[]) res.values;
      numResults = res.count;
      if (res.count > 0)
        notifyDataSetChanged();
      else
        notifyDataSetInvalidated();
    }
  }
}
