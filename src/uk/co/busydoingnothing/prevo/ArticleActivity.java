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

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.webkit.WebView;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

public class ArticleActivity extends Activity
{
  public static final String EXTRA_ARTICLE_NUMBER =
    "uk.co.busydoingnothing.prevo.ArticleNumber";
  public static final String EXTRA_MARK_NUMBER =
    "uk.co.busydoingnothing.prevo.MarkNumber";

  public static final String TAG = "prevoarticle";

  private WebView webView;

  private static void appendStream (OutputStream out,
                                    InputStream in)
    throws IOException
  {
    byte[] buf = new byte[512];
    int got;

    while ((got = in.read (buf)) != -1)
      out.write (buf, 0, got);
  }

  private void loadArticle (int article,
                            int mark)
    throws IOException
  {
    Resources resources = getResources ();
    AssetManager assetManager = getAssets ();
    ByteArrayOutputStream buf = new ByteArrayOutputStream ();

    appendStream (buf, resources.openRawResource (R.raw.article_header));
    appendStream (buf, assetManager.open ("articles/article-" + article +
                                          ".xml"));
    appendStream (buf, resources.openRawResource (R.raw.article_footer));

    StringBuilder url =
      new StringBuilder ("data:text/html;charset=UTF-8;base64,");
    url.append (Base64.encodeToString (buf.toByteArray (),
                                       Base64.NO_WRAP));

    webView.loadUrl (url.toString ());
  }

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setContentView (R.layout.article);

    this.webView = (WebView) findViewById (R.id.article_webview);

    Intent intent = getIntent ();

    if (intent != null)
      {
        int article = intent.getIntExtra (EXTRA_ARTICLE_NUMBER, -1);
        int mark = intent.getIntExtra (EXTRA_MARK_NUMBER, -1);

        if (article != -1)
          {
            try
              {
                loadArticle (article, mark);
              }
            catch (IOException e)
              {
                Log.wtf ("Error while loading an asset", e);
              }
          }
      }
  }
}
