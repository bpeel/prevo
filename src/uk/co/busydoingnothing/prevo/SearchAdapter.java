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

  private Trie trie;

  public SearchAdapter (Context context,
                        Trie trie)
  {
    this.context = context;
    this.trie = trie;

    results = new SearchResult[MAX_RESULTS];
    numResults = trie.search ("", results);
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
  public SearchFilter getFilter ()
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
      String filterString = Hats.removeHats (filter);
      SearchResult[] results = new SearchResult[MAX_RESULTS];

      ret.values = results;
      ret.count = trie.search (filterString, results);

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
