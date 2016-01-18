/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2012, 2013, 2016  Neil Roberts
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
import java.util.Arrays;
import java.util.Comparator;
import java.util.Vector;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

public class LanguageList
{
  private Language[] languagesByName;
  private Language[] languagesByCode;

  private static LanguageList defaultLanguageList;
  private LanguageCodeComparator codeComparator;

  public LanguageList (Context context)
  {
    try
      {
        XmlPullParser parser;
        StringBuilder language = new StringBuilder ();

        Vector<Language> languages = new Vector<Language> ();

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

                    languages.add (new Language (language.toString (),
                                                 code));
                  }
              }
            else if (eventType == XmlPullParser.END_DOCUMENT)
              break;

            parser.next ();
          }

        languagesByName = languages.toArray (new Language[languages.size ()]);
      }
    catch (XmlPullParserException e)
      {
        throw new IllegalStateException (e);
      }
    catch (java.io.IOException e)
      {
        throw new IllegalStateException (e);
      }

    codeComparator = new LanguageCodeComparator ();

    languagesByCode = new Language[languagesByName.length];
    for (int i = 0; i < languagesByName.length; i++)
      languagesByCode[i] = languagesByName[i];

    Arrays.sort (languagesByCode, codeComparator);
  }

  public static LanguageList getDefault (Context context)
  {
    if (defaultLanguageList == null)
      defaultLanguageList = new LanguageList (context);

    return defaultLanguageList;
  }

  public Language[] getAllLanguages ()
  {
    return languagesByName;
  }

  public String getLanguageName (String languageCode,
                                 boolean withArticle)
  {
    if (languageCode.equals ("eo"))
      return "esperanto";

    int lang = Arrays.binarySearch (languagesByCode,
                                    new Language ("", languageCode),
                                    codeComparator);

    if (lang < 0)
      return languageCode;

    String name = languagesByCode[lang].getName ();

    if (withArticle)
      return "la " + name;
    else
      return name;
  }

  public String getLanguageName (String languageCode)
  {
    return getLanguageName (languageCode, false);
  }

  private static class LanguageCodeComparator implements Comparator<Language>
  {
    public int compare (Language a,
                        Language b)
    {
      return a.getCode ().compareTo (b.getCode ());
    }
  }
}
