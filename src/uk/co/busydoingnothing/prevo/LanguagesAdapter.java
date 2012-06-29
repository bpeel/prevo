package uk.co.busydoingnothing.prevo;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;
import java.util.Vector;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

public class LanguagesAdapter extends BaseAdapter
{
  private static final String[] mainLanguages =
  { "esperanto", "angla", "franca" };

  private String[] allLanguages;

  private Context context;

  static final int TYPE_HEADER = 0;
  static final int TYPE_LANGUAGE = 1;
  static final int TYPE_COUNT = 2;

  public LanguagesAdapter (Context context)
  {
    this.context = context;
    this.allLanguages = getAllLanguages ();
  }

  private String[] getAllLanguages ()
  {
    try
      {
        XmlPullParser parser;
        StringBuilder language = new StringBuilder ();

        Vector<String> languages = new Vector<String> ();

        parser = context.getResources ().getXml (R.xml.languages);

        while (true)
          {
            int eventType = parser.getEventType ();

            if (eventType == XmlPullParser.START_TAG)
              {
                if (parser.getName ().equals ("lang"))
                  {
                    String code = parser.getAttributeValue (null, "code");
                    language.setLength (0);

                    while (true)
                      {
                        parser.next ();
                        eventType = parser.getEventType ();
                        if (eventType == XmlPullParser.END_TAG)
                          break;
                        else if (eventType == XmlPullParser.TEXT)
                          language.append (parser.getText ());
                      }

                    languages.add (language.toString ());
                  }
              }
            else if (eventType == XmlPullParser.END_DOCUMENT)
              break;

            parser.next ();
          }

        return languages.toArray (new String[languages.size ()]);
      }
    catch (XmlPullParserException e)
      {
        throw new IllegalStateException (e);
      }
    catch (java.io.IOException e)
      {
        throw new IllegalStateException (e);
      }
  }

  @Override
  public int getCount ()
  {
    return mainLanguages.length + allLanguages.length + 2;
  }

  @Override
  public Object getItem (int position)
  {
    if (position == 0)
      return "ĈEFAJ LINGVOJ";

    position--;

    if (position < mainLanguages.length)
      return mainLanguages[position];

    position -= mainLanguages.length;

    if (position == 0)
      return "ĈIUJ LINGVOJ";

    return allLanguages[position - 1];
  }

  @Override
  public long getItemId (int position)
  {
    return position;
  }

  @Override
  public int getItemViewType (int position)
  {
    if (position == 0 || position == mainLanguages.length + 1)
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
}
