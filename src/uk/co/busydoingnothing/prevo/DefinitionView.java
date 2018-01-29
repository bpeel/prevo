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

import android.util.AttributeSet;
import android.widget.TextView;
import android.content.Context;
import android.view.ContextMenu.ContextMenuInfo;

public class DefinitionView extends TextView
{
  private CharSequence word;
  private CharSequence definition;

  public class DefinitionContextMenuInfo implements ContextMenuInfo
  {
    public CharSequence word;
    public CharSequence definition;
  }

  private DefinitionContextMenuInfo contextMenuInfo;

  public DefinitionView (Context context)
  {
    super (context);
  }

  public DefinitionView (Context context, AttributeSet attrs)
  {
    super (context, attrs);
  }

  public DefinitionView (Context context, AttributeSet attrs, int defStyle)
  {
    super (context, attrs, defStyle);
  }

  public DefinitionView (Context context,
                         CharSequence word,
                         CharSequence definition)
  {
    super (context);
    setWord (word, definition);
  }

  public void setWord (CharSequence word, CharSequence definition)
  {
    this.word = word;
    this.definition = definition;
    contextMenuInfo = null;
  }

  public ContextMenuInfo getContextMenuInfo ()
  {
    if (contextMenuInfo == null)
      {
        contextMenuInfo = new DefinitionContextMenuInfo ();
        contextMenuInfo.word = this.word;
        contextMenuInfo.definition = this.definition;
      }

    return contextMenuInfo;
  }
}
