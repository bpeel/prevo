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

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

public class LanguageDatabaseHelper extends SQLiteOpenHelper
{
  static private final String DATABASE_NAME = "language";
  static private final int DATABASE_VERSION = 1;
  static private final String TABLE_CREATE =
    "create table `language` " +
    "(`code` char(3) primary key not null, " +
    "`usage_count` integer not null default 0)";
  static private final int MAX_RESULTS = 2;

  static private final String TAG = "prevo";

  private Context context;

  public LanguageDatabaseHelper (Context context)
  {
    super (context, DATABASE_NAME, null, DATABASE_VERSION);

    this.context = context;
  }

  @Override
  public void onCreate (SQLiteDatabase db)
  {
    db.execSQL (TABLE_CREATE);
  }

  @Override
  public void onUpgrade (SQLiteDatabase db,
                         int oldVersion,
                         int newVersion)
  {
    Log.wtf (TAG, "Unexpected database upgrade requested from " + oldVersion +
             "to " + newVersion);
  }

  public String[] getLanguages ()
  {
    SQLiteDatabase db = getReadableDatabase ();

    Cursor cursor = db.query ("language",
                              new String[] { "code" },
                              "`code` != 'eo'", /* where clause */
                              null, /* selection args */
                              null, /* group by */
                              null, /* having */
                              "`usage_count` desc", /* order by */
                              Integer.toString (MAX_RESULTS));

    String[] results = new String[cursor.getCount ()];
    int i = 0;

    for (cursor.moveToFirst (); !cursor.isAfterLast (); cursor.moveToNext ())
      results[i++] = cursor.getString (0);

    cursor.close ();

    db.close ();

    return results;
  }

  public void useLanguage (String code)
  {
    SQLiteDatabase db = getWritableDatabase ();

    db.execSQL ("insert or ignore into `language` " +
                "(`code`, `usage_count`) values " +
                "(?, ?)",
                new Object[] { code, 0 });
    db.execSQL ("update `language` set `usage_count` = `usage_count` + 1 " +
                "where `code` = ?",
                new Object[] { code });

    db.close ();
  }
}
