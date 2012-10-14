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

import android.widget.TextView;
import android.content.Context;
import android.view.ContextMenu.ContextMenuInfo;

public class DefinitionView extends TextView
{
  public class DefinitionContextMenuInfo implements ContextMenuInfo
  {
    public CharSequence text;
  }

  private DefinitionContextMenuInfo contextMenuInfo;

  public DefinitionView (Context context)
  {
    super (context);
  }

  public ContextMenuInfo getContextMenuInfo ()
  {
    if (contextMenuInfo == null)
      contextMenuInfo = new DefinitionContextMenuInfo ();

    contextMenuInfo.text = getText ();

    return contextMenuInfo;
  }
}
