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
import android.util.AttributeSet;
import android.view.View;
import android.widget.ScrollView;

/* This is the same as a ScrollView except that it can set a scrollTo
 * to occur after a layout has occured. If you don't wait until after
 * the layout has occured then the views won't have the right
 * position */

public class DelayedScrollView extends ScrollView
{
  private boolean hadLayout = false;
  private View delayedView = null;

  public DelayedScrollView (Context context)
  {
    super (context);
  }

  public DelayedScrollView (Context context, AttributeSet attrs)
  {
    super (context, attrs);
  }

  public DelayedScrollView (Context context, AttributeSet attrs, int defStyle)
  {
    super (context, attrs, defStyle);
  }

  public void delayedScrollTo (View view)
  {
    if (hadLayout)
      {
        scrollTo (0, view.getTop ());
        delayedView = null;
      }
    else
      delayedView = view;
  }

  @Override
  protected void onLayout (boolean changed,
                           int left,
                           int top,
                           int right,
                           int bottom)
  {
    super.onLayout (changed, left, top, right, bottom);

    hadLayout = true;

    if (delayedView != null)
      delayedScrollTo (delayedView);
  }
}
