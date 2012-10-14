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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.view.ContextMenu;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.QuoteSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.StyleSpan;
import android.text.style.SuperscriptSpan;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.InputStream;
import java.io.IOException;
import java.util.Vector;

public class ArticleActivity extends Activity
{
  public static final String EXTRA_ARTICLE_NUMBER =
    "uk.co.busydoingnothing.prevo.ArticleNumber";
  public static final String EXTRA_MARK_NUMBER =
    "uk.co.busydoingnothing.prevo.MarkNumber";

  /* http://www.openintents.org/en/node/720 */
  /* The second one is used by Anki 1.1.3 and the first one only works
   * in Anki 2.0 so we just try both */
  private static final String[] ACTION_CREATE_FLASHCARD =
  {
    "org.openintents.action.CREATE_FLASHCARD",
    "org.openintents.indiclash.CREATE_FLASHCARD"
  };
  public static final String SOURCE_TEXT = "SOURCE_TEXT";
  public static final String TARGET_TEXT = "TARGET_TEXT";

  public static final String TAG = "prevoarticle";

  public static final int DIALOG_NO_FLASHCARD = 0x10c;

  private Vector<TextView> sectionHeaders;

  private DelayedScrollView scrollView;
  private int articleNumber;

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
      new SpannableString (new String (utf8String));

    int spanLength;

    while ((spanLength = readShort (in)) != 0)
      {
        int spanStart = readShort (in);
        int data1 = readShort (in);
        final int data2 = readShort (in);
        int spanType = in.read ();

        if (spanType == -1)
          throwEOF ();

        if (spanStart < 0 || spanLength < 0 ||
            spanStart + spanLength > string.length ())
          Log.wtf (TAG,
                   "Invalid span " +
                   spanStart +
                   "→" +
                   (spanLength + spanStart) +
                   " for string of length " +
                   string.length ());

        switch (spanType)
          {
          case 0:
            {
              ClickableSpan span;

              if (data1 == this.articleNumber)
                {
                  span = (new ClickableSpan ()
                    {
                      @Override
                      public void onClick (View widget)
                      {
                        showSection (data2);
                      }
                    });
                }
              else
                {
                  span = new ReferenceSpan (data1, data2);
                }

              string.setSpan (span, spanStart, spanStart + spanLength, 0);
            }
            break;

          case 1:
            string.setSpan (new SuperscriptSpan (),
                            spanStart,
                            spanStart + spanLength,
                            0 /* flags */);
            string.setSpan (new RelativeSizeSpan (0.5f),
                            spanStart,
                            spanStart + spanLength,
                            0 /* flags */);
            break;

          case 2:
            string.setSpan (new StyleSpan (android.graphics.Typeface.ITALIC),
                            spanStart,
                            spanStart + spanLength,
                            0 /* flags */);
            break;

          case 3:
            string.setSpan (new QuoteSpan (),
                            spanStart,
                            spanStart + spanLength,
                            0 /* flags */);
            break;

          case 4:
            string.setSpan (new StyleSpan (android.graphics.Typeface.BOLD),
                            spanStart,
                            spanStart + spanLength,
                            0 /* flags */);
            break;
          }
      }

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

  private LinearLayout loadArticle (int article)
    throws IOException
  {
    AssetManager assetManager = getAssets ();
    InputStream in = assetManager.open ("articles/article-" + article + ".bin");

    setTitle (readSpannableString (in));

    LinearLayout layout = new LinearLayout (this);
    layout.setOrientation (LinearLayout.VERTICAL);

    SpannableString str;
    SpannableString lastTitle = null;
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
            sectionHeaders.add (tv);
            lastTitle = str;
          }
        else
          {
            tv = new DefinitionView (this, lastTitle, str);
            isTitle = true;

            registerForContextMenu (tv);
          }

        tv.setMovementMethod (LinkMovementMethod.getInstance ());
        tv.setText (str, TextView.BufferType.SPANNABLE);

        layout.addView (tv);
      }

    return layout;
  }

  private void showSection (int section)
  {
    int ypos = 0;

    Log.i (TAG, "Showing section " + section + " of article " + articleNumber);

    scrollView.delayedScrollTo (sectionHeaders.get (section));
  }

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);

    Intent intent = getIntent ();
    scrollView = new DelayedScrollView (this);

    setContentView (scrollView);
    sectionHeaders = new Vector<TextView> ();

    if (intent != null)
      {
        int article = intent.getIntExtra (EXTRA_ARTICLE_NUMBER, -1);
        int mark = intent.getIntExtra (EXTRA_MARK_NUMBER, -1);

        if (article != -1)
          {
            try
              {
                this.articleNumber = article;
                scrollView.addView (loadArticle (article));
                showSection (mark);
              }
            catch (IOException e)
              {
                Log.wtf ("Error while loading an asset", e);
              }
          }
      }
  }

  @Override
  public boolean onCreateOptionsMenu (Menu menu)
  {
    MenuInflater inflater = getMenuInflater ();

    inflater.inflate (R.menu.other_menu, menu);

    return true;
  }

  @Override
  public boolean onOptionsItemSelected (MenuItem item)
  {
    if (MenuHelper.onOptionsItemSelected (this, item))
      return true;

    return super.onOptionsItemSelected (item);
  }

  @Override
  protected Dialog onCreateDialog (int id)
  {
    Resources res = getResources ();

    switch (id)
      {
      case DIALOG_NO_FLASHCARD:
        {
          AlertDialog.Builder builder = new AlertDialog.Builder (this);

          LayoutInflater layoutInflater = getLayoutInflater ();
          TextView tv =
            (TextView) layoutInflater.inflate (R.layout.no_flashcard_view,
                                               null);
          builder
            .setView (tv)
            .setCancelable (true)
            .setNegativeButton (R.string.close,
                                new DialogInterface.OnClickListener ()
                                {
                                  @Override
                                  public void onClick (DialogInterface dialog,
                                                       int whichButton)
                                  {
                                  }
                                });
          return builder.create ();
        }

      default:
        return MenuHelper.onCreateDialog (this, id);
      }
  }

  @Override
  public boolean onKeyDown (int keyCode,
                            KeyEvent event)
  {
    if (keyCode == KeyEvent.KEYCODE_SEARCH)
      {
        MenuHelper.goSearch (this);
        return true;
      }

    return super.onKeyDown (keyCode, event);
  }

  @Override
  public void onCreateContextMenu (ContextMenu menu,
                                   View v,
                                   ContextMenu.ContextMenuInfo menuInfo)
  {
    super.onCreateContextMenu (menu, v, menuInfo);

    if (v instanceof TextView)
      {
        MenuInflater inflater = getMenuInflater ();
        inflater.inflate (R.menu.definition_menu, menu);
      }
  }

  private void createFlashcard (CharSequence sourceText,
                                CharSequence targetText)
  {
    Intent intent = new Intent ();
    int i;

    intent.putExtra (SOURCE_TEXT, sourceText.toString ());
    intent.putExtra (TARGET_TEXT, targetText.toString ());

    for (i = 0; i < ACTION_CREATE_FLASHCARD.length; i++)
      {
        intent.setAction (ACTION_CREATE_FLASHCARD[i]);

        try
          {
            startActivity (intent);
            break;
          }
        catch (android.content.ActivityNotFoundException e)
          {
            Log.i (TAG, "Failed to start activity: " + e.getMessage ());
          }
      }

    if (i >= ACTION_CREATE_FLASHCARD.length)
      showDialog (DIALOG_NO_FLASHCARD);
  }

  @Override
  public boolean onContextItemSelected (MenuItem item)
  {
    ContextMenu.ContextMenuInfo info = item.getMenuInfo ();

    if (info instanceof DefinitionView.DefinitionContextMenuInfo)
      {
        DefinitionView.DefinitionContextMenuInfo defInfo =
          (DefinitionView.DefinitionContextMenuInfo) info;

        switch (item.getItemId())
          {
          case R.id.menu_copy_definition:
            CharSequence label =
              getResources ().getText (R.string.definition_label);

            SpannedCopy.copyText (this, label, defInfo.definition);

            return true;

          case R.id.menu_create_flashcard_word:
            createFlashcard (defInfo.definition, defInfo.word);
            return true;

          case R.id.menu_create_flashcard_definition:
            createFlashcard (defInfo.word, defInfo.definition);
            return true;
          }
      }

    return super.onContextItemSelected(item);
  }
}
