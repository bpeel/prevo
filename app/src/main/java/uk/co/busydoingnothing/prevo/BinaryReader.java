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

import java.io.InputStream;
import java.io.IOException;

public class BinaryReader
{
  private long position = 0;
  private InputStream in;
  private byte numberBuffer[] = new byte[4];

  public BinaryReader (InputStream in)
  {
    this.in = in;
  }

  private static void throwEOF ()
    throws IOException
  {
    throw new IOException ("Unexpected EOF");
  }

  public void readAll (byte[] array,
                       int offset,
                       int length)
    throws IOException
  {
    while (length > 0)
      {
        int got = in.read (array, offset, length);

        if (got == -1)
          throwEOF ();

        position += got;

        offset += got;
        length -= got;
      }
  }

  public int readByte ()
    throws IOException
  {
    int ret = in.read ();

    if (ret == -1)
      throwEOF ();

    position += 1;

    return ret;
  }

  public void readAll (byte[] array)
    throws IOException
  {
    readAll (array, 0, array.length);
  }

  public int readShort ()
    throws IOException
  {
    readAll (numberBuffer, 0, 2);

    return (numberBuffer[0] & 0xff) | ((numberBuffer[1] & 0xff) << 8);
  }

  public int readInt ()
    throws IOException
  {
    readAll (numberBuffer, 0, 4);

    return ((numberBuffer[0] & 0xff) |
            ((numberBuffer[1] & 0xff) << 8) |
            ((numberBuffer[2] & 0xff) << 16) |
            ((numberBuffer[3] & 0xff) << 24));
  }

  public long getPosition ()
  {
    return position;
  }

  public void skip (long byteCount)
    throws IOException
  {
    long skipped;

    /* First try skipping with the input stream's method */
    skipped = in.skip (byteCount);
    position += skipped;
    byteCount -= skipped;

    /* If that doesn't work we'll try manually reading into a buffer */
    if (byteCount > 0)
      {
        byte buf[] = new byte[1024];

        do
          {
            int toRead;

            if (buf.length < byteCount)
              toRead = buf.length;
            else
              toRead = (int) byteCount;

            readAll (buf, 0, toRead);

            byteCount -= toRead;
            position += toRead;
          }
        while (byteCount > 0);
      }
  }
}
