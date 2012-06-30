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

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
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

    lv.setOnItemClickListener (new AdapterView.OnItemClickListener ()
      {
        public void onItemClick (AdapterView<?> parent,
                                 View view,
                                 int position,
                                 long id)
        {
          SearchAdapter adapter =
            (SearchAdapter) parent.getAdapter ();
          SearchResult result = adapter.getItem (position);
          Intent intent = new Intent (view.getContext (),
                                      ArticleActivity.class);
          intent.putExtra (ArticleActivity.EXTRA_ARTICLE_NUMBER,
                           result.getArticle ());
          intent.putExtra (ArticleActivity.EXTRA_MARK_NUMBER,
                           result.getMark ());
          startActivity (intent);
        }
      });
  }
}
