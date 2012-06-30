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

import java.io.FileInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.nio.charset.Charset;

class TrieStack
{
  private int[] data;
  private int size;

  public TrieStack ()
  {
    this.data = new int[64];
    this.size = 0;
  }

  public int getTopStart ()
  {
    return data[size - 3];
  }

  public int getTopEnd ()
  {
    return data[size - 2];
  }

  public int getTopStringLength ()
  {
    return data[size - 1];
  }

  public void pop ()
  {
    size -= 3;
  }

  public boolean isEmpty ()
  {
    return size <= 0;
  }

  public void push (int start,
                    int end,
                    int stringLength)
  {
    /* If there isn't enough space in the array then we'll double its
     * size. The size of the array is initially chosen to be quite
     * large so this should probably never happen */
    if (size + 3 >= data.length)
      {
        int[] newData = new int[data.length * 2];
        System.arraycopy (data, 0, newData, 0, data.length);
        data = newData;
      }

    data[size++] = start;
    data[size++] = end;
    data[size++] = stringLength;
  }
}

public class Trie
{
  private Charset utf8Charset;
  private byte data[];

  private static void readAll (InputStream stream,
                               byte[] data,
                               int offset,
                               int length)
    throws IOException
  {
    while (length > 0)
      {
        int got = stream.read (data, offset, length);

        if (got == -1)
          throw new IOException ("Unexpected end of file");
        else
          {
            offset += got;
            length -= got;
          }
      }
  }

  private static final int extractInt (byte[] data,
                                       int offset)
  {
    return (((data[offset + 0] & 0xff) << 0) |
            ((data[offset + 1] & 0xff) << 8) |
            ((data[offset + 2] & 0xff) << 16) |
            ((data[offset + 3] & 0xff) << 24));
  }

  public Trie (InputStream dataStream)
    throws IOException
  {
    byte lengthBytes[] = new byte[4];
    int totalLength;

    /* Read 4 bytes to get the length of the file */
    readAll (dataStream, lengthBytes, 0, lengthBytes.length);
    totalLength = extractInt (lengthBytes, 0);

    /* Create a byte array big enough to hold the entire file and copy
     * the length we just read into the beginning */
    data = new byte[totalLength];
    System.arraycopy (lengthBytes, 0, data, 0, 4);

    /* Read the rest of the data */
    readAll (dataStream, data, 4, totalLength - 4);

    utf8Charset = Charset.forName ("UTF-8");
  }

  /* Gets the number of bytes needed for a UTF-8 sequence which begins
   * with the given byte */
  private static int getUtf8Length (byte firstByte)
  {
    if (firstByte >= 0)
      return 1;
    if ((firstByte & 0xe0) == 0xc0)
      return 2;
    if ((firstByte & 0xf0) == 0xe0)
      return 3;
    if ((firstByte & 0xf8) == 0xf0)
      return 4;
    if ((firstByte & 0xfc) == 0xf8)
      return 5;

    return 6;
  }

  private static boolean compareArray (byte[] a,
                                       int aOffset,
                                       byte[] b,
                                       int bOffset,
                                       int length)
  {
    while (length-- > 0)
      if (a[aOffset++] != b[bOffset++])
        return false;

    return true;
  }

  private String getCharacter (int offset)
  {
    return new String (data, offset, getUtf8Length (data[offset]));
  }

  /* Searches the trie for words that begin with 'prefix'. The results
   * array is filled with the results. If more results are available
   * than the length of the results array then they are ignored. If
   * less are available then the remainder of the array is untouched.
   * The method returns the number of results found */
  public int search (String prefix,
                     SearchResult[] results)
  {
    /* Convert the string to unicode to make it easier to compare with
     * the unicode characters in the trie */
    byte[] prefixBytes = prefix.getBytes (utf8Charset);

    int trieStart = 0;
    int prefixOffset = 0;

    while (prefixOffset < prefixBytes.length)
      {
        int characterLen = getUtf8Length (prefixBytes[prefixOffset]);
        int childStart;

        /* Get the total length of this node */
        int offset = extractInt (data, trieStart);

        /* Skip the character for this node */
        childStart = trieStart + 4;
        childStart += getUtf8Length (data[childStart]);

        /* If the high bit in the offset is set then it is followed by
         * the article and mark number which we want to skip */
        if (offset < 0)
          {
            offset &= 0x7fffffff;
            childStart += 3;
          }

        int trieEnd = trieStart + offset;

        trieStart = childStart;

        /* trieStart is now pointing into the children of the
         * selected node. We'll scan over these until we either find a
         * matching character for the next character of the prefix or
         * we hit the end of the node */
        while (true)
          {
            /* If we've reached the end of the node then we haven't
             * found a matching character for the prefix so there are
             * no results */
            if (trieStart >= trieEnd)
              return 0;

            /* If we've found a matching character then start scanning
             * into this node */
            if (compareArray (prefixBytes, prefixOffset,
                              data, trieStart + 4,
                              characterLen))
              break;
            /* Otherwise skip past the node to the next sibling */
            else
              trieStart += extractInt (data, trieStart) & 0x7fffffff;
          }

        prefixOffset += characterLen;
      }

    StringBuilder stringBuf = new StringBuilder (prefix);

    /* trieStart is now pointing at the last node with this string.
     * Any children of that node are therefore extensions of the
     * prefix. We can now depth-first search the tree to get them all
     * in sorted order */

    TrieStack stack = new TrieStack ();

    stack.push (trieStart,
                trieStart + extractInt (data, trieStart) & 0x7fffffff,
                stringBuf.length ());

    int numResults = 0;
    boolean firstChar = true;

    while (numResults < results.length &&
           !stack.isEmpty ())
      {
        int searchStart = stack.getTopStart ();
        int searchEnd = stack.getTopEnd ();

        stringBuf.setLength (stack.getTopStringLength ());

        stack.pop ();

        int offset = extractInt (data, searchStart);
        int characterLen = getUtf8Length (data[searchStart + 4]);
        int childrenStart = searchStart + 4 + characterLen;
        int oldLength = stringBuf.length ();

        if (firstChar)
          firstChar = false;
        else
          stringBuf.append (new String (data,
                                        searchStart + 4,
                                        characterLen,
                                        utf8Charset));

        /* If this is a complete word then add it to the results */
        if (offset < 0)
          {
            int article = ((data[childrenStart] & 0xff) |
                           ((data[childrenStart + 1] & 0xff) << 8));
            int mark = data[childrenStart + 2] & 0xff;

            results[numResults++] = new SearchResult (stringBuf.toString (),
                                                      article,
                                                      mark);

            childrenStart += 3;
            offset &= 0x7fffffff;
          }

        /* If there is a sibling then make sure we continue from that
         * after we've descended through the children of this node */
        if (searchStart + offset < searchEnd)
          stack.push (searchStart + offset, searchEnd, oldLength);

        /* Push a search for the children of this node */
        if (childrenStart < searchStart + offset)
            stack.push (childrenStart,
                        searchStart + offset,
                        stringBuf.length ());
      }

    return numResults;
  }

  /* Test program */
  public static void main (String[] args)
    throws IOException
  {
    if (args.length != 2)
      {
        System.err.println ("Usage: java Trie <index> <prefix>");
        System.exit (1);
      }

    FileInputStream inputStream = new FileInputStream (args[0]);
    Trie trie = new Trie (inputStream);

    SearchResult result[] = new SearchResult[100];

    int numResults = trie.search (args[1], result);

    for (int i = 0; i < numResults; i++)
      System.out.println (result[i].getWord () + ": " +
                          result[i].getArticle () + "," +
                          result[i].getMark ());
  }
}
