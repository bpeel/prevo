package uk.co.busydoingnothing.prevo;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.widget.ListView;
import java.io.InputStream;

public class SearchActivity extends ListActivity
{
  public static final String EXTRA_LANGUAGE =
    "uk.co.busydoingnothing.prevo.Language";

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setContentView (R.layout.search);

    Intent intent = getIntent ();

    ListView lv = getListView ();

    if (intent != null)
      {
        String language = intent.getStringExtra (EXTRA_LANGUAGE);

        if (language != null)
          {
            try
              {
                InputStream indexIn =
                  getAssets ().open ("indices/index-" + language + ".bin");
                Trie trie = new Trie (indexIn);
                SearchAdapter adapter = new SearchAdapter (this, trie);
                setListAdapter (adapter);

                lv.setTextFilterEnabled (true);
              }
            catch (java.io.IOException e)
              {
                throw new IllegalStateException ("Error while loading " +
                                                 "an asset");
              }
          }
      }
  }
}
