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
import android.support.design.widget.CoordinatorLayout;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.SharedPreferences;
import android.support.v7.widget.Toolbar;
import android.view.ContextMenu;
import android.os.Bundle;
import android.os.Handler;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.QuoteSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.StyleSpan;
import android.text.style.SuperscriptSpan;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.KeyEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.widget.AdapterView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.ZoomControls;
import java.io.IOException;
import java.util.Vector;
import java.util.Locale;

public class ArticleActivity extends AppCompatActivity
  implements SharedPreferences.OnSharedPreferenceChangeListener
{
  public static final String EXTRA_ARTICLE_NUMBER =
    "uk.co.busydoingnothing.prevo.ArticleNumber";
  public static final String EXTRA_MARK_NUMBER =
    "uk.co.busydoingnothing.prevo.MarkNumber";

  /* http://www.openintents.org/en/node/720 */
  /* This probably only works with AnkiDroid 2.0 */
  private static final String ACTION_CREATE_FLASHCARD =
    "org.openintents.action.CREATE_FLASHCARD";

  public static final String SOURCE_TEXT = "SOURCE_TEXT";
  public static final String TARGET_TEXT = "TARGET_TEXT";

  public static final String TAG = "prevoarticle";

  public static final int DIALOG_NO_FLASHCARD = 0x10c;

  private Vector<TextView> sectionHeaders;
  private Vector<TextView> definitions;

  private DelayedScrollView scrollView;
  private View articleView;
  private int articleNumber;

  private CoordinatorLayout layout;

  /* There are 10 font sizes ranging from 0 to 9. The actual font size
   * used is calculated from a logarithmic scale and set in density
   * independent pixels */
  private static final int N_FONT_SIZES = 10;
  private static final float FONT_SIZE_ROOT = 1.2f;

  private int fontSize = N_FONT_SIZES / 2;
  private float titleBaseTextSize;
  private float definitionBaseTextSize;

  private Handler handler;

  private boolean stopped;
  private boolean reloadQueued;

  private void skipSpannableString (BinaryReader in)
    throws IOException
  {
    int strLength = in.readShort ();

    in.skip (strLength);

    int spanLength;

    while ((spanLength = in.readShort ()) != 0)
      in.skip (2 + 2 + 2 + 1);
  }

  private SpannableString readSpannableString (BinaryReader in)
    throws IOException
  {
    int strLength = in.readShort ();
    byte[] utf8String = new byte[strLength];

    in.readAll(utf8String);

    SpannableString string =
      new SpannableString (new String (utf8String));

    int spanLength;

    while ((spanLength = in.readShort ()) != 0)
      {
        int spanStart = in.readShort ();
        int data1 = in.readShort ();
        final int data2 = in.readShort ();
        int spanType = in.readByte ();

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

  private String readString (BinaryReader in,
                             int maxLength)
    throws IOException
  {
    byte buf[] = new byte[maxLength];

    in.readAll (buf);

    int len = 3;
    while (len > 0 && buf[len - 1] == '\0')
      len--;

    return new String (buf, 0, len);
  }

  private void skipArticles (BinaryReader in,
                             int numArticles)
    throws IOException
  {
    char buf[] = null;

    for (int i = 0; i < numArticles; i++)
      {
        int articleLength = in.readInt ();

        in.skip (articleLength);
      }
  }

  private LinearLayout loadArticle(int article)
          throws IOException {
    AssetManager assetManager = getAssets();
    String filename = String.format (Locale.US,
                                     "articles/article-%03xx.bin",
            article >> 4);
    BinaryReader in = new BinaryReader (assetManager.open (filename));

    skipArticles(in, article & 0xf);

    int articleLength = in.readInt ();
    long articleStart = in.getPosition ();

    setTitle (readSpannableString (in));

    LinearLayout layout = new LinearLayout (this);
    layout.setOrientation (LinearLayout.VERTICAL);
    // Add some space to make sure FAB does hinder readability
    // TODO: value should be <margin> + <button size> display pixels
    // <margin> is 16 dp on phones, 24 dp on tablets
    // <button size> is 56 dp
    layout.setPadding(0, 0, 0, 2*(16 + 56));

    LayoutInflater layoutInflater = getLayoutInflater ();
    TextView tv[] = new TextView[2];

    SelectedLanguages selectedLanguages = new SelectedLanguages (this);

    while (in.getPosition () - articleStart < articleLength)
      {
        String languageCode = readString (in, 3);

        if (selectedLanguages.contains (languageCode))
          {
            SpannableString header = readSpannableString (in);
            SpannableString content = readSpannableString (in);

            tv[0] = (TextView) layoutInflater.inflate (R.layout.section_header,
                                                       layout,
                                                       false);
            sectionHeaders.add (tv[0]);
            titleBaseTextSize = tv[0].getTextSize ();
            tv[0].setText (header, TextView.BufferType.SPANNABLE);

            tv[1] = new DefinitionView (this, header, content);
            definitions.add (tv[1]);
            tv[1].setText (content, TextView.BufferType.SPANNABLE);
            registerForContextMenu (tv[1]);
            definitionBaseTextSize = tv[1].getTextSize ();

            for (int i = 0; i < tv.length; i++)
              {
                tv[i].setMovementMethod (LinkMovementMethod.getInstance ());
                layout.addView (tv[i]);
              }
          }
        else
          {
            skipSpannableString (in);
            skipSpannableString (in);
          }
      }

    return layout;
  }

  private void showSection (int section)
  {
    int ypos = 0;

    Log.i (TAG, "Showing section " + section + " of article " + articleNumber);

    scrollView.delayedScrollTo (sectionHeaders.get (section));
  }

  private void setFontSize (int fontSize)
  {
    if (fontSize < 0)
      fontSize = 0;
    else if (fontSize >= N_FONT_SIZES)
      fontSize = N_FONT_SIZES - 1;

    if (fontSize != this.fontSize)
      {
        /* There's no point in updating the font size if a reload is
         * queued because it will just get set back to the default
         * when it is finally reloaded */
        if (!reloadQueued)
          {
            float fontSizeScale =
              (float) Math.pow (FONT_SIZE_ROOT,
                                fontSize - N_FONT_SIZES / 2);
            float titleFontSize = titleBaseTextSize * fontSizeScale;
            float definitionFontSize = definitionBaseTextSize * fontSizeScale;

            for (TextView tv : sectionHeaders)
              tv.setTextSize (TypedValue.COMPLEX_UNIT_PX, titleFontSize);

            for (TextView tv : definitions)
              tv.setTextSize (TypedValue.COMPLEX_UNIT_PX, definitionFontSize);
          }

        this.fontSize = fontSize;
      }
  }

  private void loadIntendedArticle ()
  {
    Intent intent = getIntent ();

    sectionHeaders.setSize (0);
    definitions.setSize (0);

    if (intent != null)
      {
        int article = intent.getIntExtra (EXTRA_ARTICLE_NUMBER, -1);
        int mark = intent.getIntExtra (EXTRA_MARK_NUMBER, -1);

        if (article != -1)
          {
            try
              {
                this.articleNumber = article;
                if (articleView != null)
                  scrollView.removeView (articleView);
                articleView = loadArticle (article);
                scrollView.addView (articleView);
                showSection (mark);
              }
            catch (IOException e)
              {
                Log.wtf ("Error while loading an asset", e);
              }
          }
      }

    /* The font size will have been reset to the default so we need to
     * update it */
    int oldFontSize = this.fontSize;
    this.fontSize = N_FONT_SIZES / 2;
    setFontSize (oldFontSize);
  }

  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);

    setContentView (R.layout.article);
    Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
    setSupportActionBar(toolbar);

    scrollView = (DelayedScrollView) findViewById (R.id.article_scroll_view);
    layout = (CoordinatorLayout) findViewById (R.id.article_layout);
    scrollView.setScaleGestureDetector(new ScaleGestureDetector(this, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
      @Override
      public void onScaleEnd(ScaleGestureDetector detector) {
        float scale = detector.getScaleFactor();

        if (scale >= 1.0) {
          zoom(+1);
        } else {
          zoom(-1);
        }
      }
    }));

    sectionHeaders = new Vector<TextView> ();
    definitions = new Vector<TextView> ();

    stopped = true;
    reloadQueued = true;

    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            AppCompatActivity.MODE_PRIVATE);

    setFontSize (prefs.getInt (MenuHelper.PREF_FONT_SIZE, fontSize));

    prefs.registerOnSharedPreferenceChangeListener(this);
  }

  @Override
  public void onStart ()
  {
    super.onStart();

    stopped = false;

    if (reloadQueued)
      {
        reloadQueued = false;
        loadIntendedArticle ();
      }
  }

  @Override
  public void onStop ()
  {
    stopped = true;

    super.onStop ();
  }

  @Override
  public void onDestroy ()
  {
    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            AppCompatActivity.MODE_PRIVATE);

    prefs.unregisterOnSharedPreferenceChangeListener (this);

    super.onDestroy();
  }

  @Override
  public boolean onCreateOptionsMenu (Menu menu)
  {
    MenuInflater inflater = getMenuInflater();

    inflater.inflate(R.menu.article_menu, menu);

    return true;
  }

  private void zoom (int direction)
  {
    int fontSize = this.fontSize + direction;

    if (fontSize >= N_FONT_SIZES)
      fontSize = N_FONT_SIZES - 1;
    else if (fontSize < 0)
      fontSize = 0;

    SharedPreferences prefs =
      getSharedPreferences (MenuHelper.PREVO_PREFERENCES,
                            AppCompatActivity.MODE_PRIVATE);
    SharedPreferences.Editor editor = prefs.edit ();
    editor.putInt (MenuHelper.PREF_FONT_SIZE, fontSize);
    editor.commit ();

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
        return null;
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
        inflater.inflate(R.menu.definition_menu, menu);
      }
  }

  private void createFlashcard (CharSequence sourceText,
                                CharSequence targetText)
  {
    Intent intent = new Intent ();
    int i;

    intent.putExtra (SOURCE_TEXT, sourceText.toString ());
    intent.putExtra (TARGET_TEXT, targetText.toString ());

    intent.setAction(ACTION_CREATE_FLASHCARD);

    try
      {
        startActivity (intent);
      }
    catch (android.content.ActivityNotFoundException e)
      {
        Log.i (TAG, "Failed to start activity: " + e.getMessage ());
        showDialog (DIALOG_NO_FLASHCARD);
      }
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

  @Override
  public void onSharedPreferenceChanged (SharedPreferences prefs,
                                         String key)
  {
    if (key.equals (MenuHelper.PREF_FONT_SIZE))
      setFontSize (prefs.getInt (MenuHelper.PREF_FONT_SIZE, fontSize));
    else if (key.equals (SelectedLanguages.PREF))
      {
        if (stopped)
          /* Queue the reload for the next time the activity is started */
          reloadQueued = true;
        else
          {
            reloadQueued = false;
            loadIntendedArticle ();
          }
      }
  }

  public void fab_onClick(View view) {
    MenuHelper.goSearch(this);
  }
}
