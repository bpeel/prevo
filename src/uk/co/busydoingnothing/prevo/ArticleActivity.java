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
import android.text.SpannableString;
import android.util.Log;
import android.view.LayoutInflater;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.Charset;

public class ArticleActivity extends Activity
{
  public static final String EXTRA_ARTICLE_NUMBER =
    "uk.co.busydoingnothing.prevo.ArticleNumber";
  public static final String EXTRA_MARK_NUMBER =
    "uk.co.busydoingnothing.prevo.MarkNumber";

  public static final String TAG = "prevoarticle";

  private Charset utf8Charset = Charset.forName ("UTF-8");

  private static void throwEOF ()
    throws IOException
  {
    throw new IOException ("Unexpected EOF");
  }

  private boolean maybeReadAll (InputStream in,
                                byte[] array,
                                int offset,
                                int length)
    throws IOException
  {
    while (length > 0)
      {
        int got = in.read (array, offset, length);

        if (got == -1)
          return false;

        offset += got;
        length -= got;
      }

    return true;
  }

  private boolean maybeReadAll (InputStream in,
                                byte[] array)
    throws IOException
  {
    return maybeReadAll (in, array, 0, array.length);
  }

  private void readAll (InputStream in,
                        byte[] array,
                        int offset,
                        int length)
    throws IOException
  {
    if (!maybeReadAll (in, array, offset, length))
      throwEOF ();
  }

  private void readAll (InputStream in,
                        byte[] array)
    throws IOException
  {
    readAll (in, array, 0, array.length);
  }

  private int maybeReadShort (InputStream in)
    throws IOException
  {
    byte[] shortBuf = new byte[2];
    if (maybeReadAll (in, shortBuf))
      return (shortBuf[0] & 0xff) | ((shortBuf[1] & 0xff) << 8);
    else
      return -1;
  }

  private int readShort (InputStream in)
    throws IOException
  {
    int val = maybeReadShort (in);

    if (val == -1)
      throwEOF ();

    return val;
  }

  private SpannableString maybeReadSpannableString (InputStream in)
    throws IOException
  {
    int strLength = maybeReadShort (in);

    if (strLength == -1)
      return null;

    byte[] utf8String = new byte[strLength];

    readAll (in, utf8String);

    SpannableString string =
      new SpannableString (new String (utf8String, utf8Charset));

    while (readShort (in) != 0)
      if (in.read () == -1)
        throwEOF ();

    return string;
  }

  private SpannableString readSpannableString (InputStream in)
    throws IOException
  {
    SpannableString val = maybeReadSpannableString (in);

    if (val == null)
      throwEOF ();

    return val;
  }

  private LinearLayout loadArticle (int article,
                                    int mark)
    throws IOException
  {
    AssetManager assetManager = getAssets ();
    InputStream in = assetManager.open ("articles/article-" + article + ".bin");

    setTitle (readSpannableString (in));

    LinearLayout layout = new LinearLayout (this);
    layout.setOrientation (LinearLayout.VERTICAL);

    SpannableString str;
    boolean isTitle = true;
    LayoutInflater layoutInflater = getLayoutInflater ();

    while ((str = maybeReadSpannableString (in)) != null)
      {
        TextView tv;

        if (isTitle)
          {
            tv = (TextView) layoutInflater.inflate (R.layout.section_header,
                                                    layout,
                                                    false);
            isTitle = false;
          }
        else
          {
            tv = new TextView (this);
            isTitle = true;
          }

        tv.setText (str, TextView.BufferType.SPANNABLE);

        layout.addView (tv);
      }

    return layout;
  }

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);

    Intent intent = getIntent ();
    ScrollView scrollView = new ScrollView (this);

    setContentView (scrollView);

    if (intent != null)
      {
        int article = intent.getIntExtra (EXTRA_ARTICLE_NUMBER, -1);
        int mark = intent.getIntExtra (EXTRA_MARK_NUMBER, -1);

        if (article != -1)
          {
            try
              {
                scrollView.addView (loadArticle (article, mark));
              }
            catch (IOException e)
              {
                Log.wtf ("Error while loading an asset", e);
              }
          }
      }
  }
}
