/*
 * PReVo - A portable version of ReVo for Android
 * Copyright (C) 2016  Neil Roberts
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
import java.io.InputStream;
import java.io.IOException;
import java.util.Iterator;
import java.util.LinkedList;

class TrieCacheEntry
{
  public String language;
  public Trie trie;
}

public class TrieCache
{
  private static final int CACHE_LENGTH = 5;
  private static LinkedList<TrieCacheEntry> entries =
    new LinkedList<TrieCacheEntry> ();

  private static Trie loadTrie (Context context,
                                String language)
    throws IOException
  {
    InputStream indexIn =
      context.getAssets ().open ("indices/index-" + language + ".bin");
    return new Trie (indexIn);
  }

  public static Trie getTrie (Context context,
                              String language)
    throws IOException
  {
    TrieCacheEntry foundEntry;

    synchronized (entries)
      {
        getEntry:
        {
          for (Iterator<TrieCacheEntry> it = entries.iterator ();
               it.hasNext ();)
            {
              TrieCacheEntry entry = it.next ();

              if (entry.language.equals (language))
                {
                  foundEntry = entry;
                  /* Remove the entry so we can add it back to the
                   * beginning */
                  it.remove ();
                  break getEntry;
                }
            }

          Trie trie = loadTrie (context, language);
          foundEntry = new TrieCacheEntry ();
          foundEntry.language = language;
          foundEntry.trie = trie;

          if (entries.size () >= CACHE_LENGTH)
            entries.removeLast ();
        }

        entries.addFirst (foundEntry);
      }

    return foundEntry.trie;
  }
}
