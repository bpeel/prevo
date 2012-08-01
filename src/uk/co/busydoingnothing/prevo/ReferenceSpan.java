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

import android.content.Intent;
import android.text.style.ClickableSpan;
import android.util.Log;
import android.view.View;

public class ReferenceSpan extends ClickableSpan
{
  private int articleNumber;
  private int sectionNumber;

  private static String TAG = "PrevoReference";

  public ReferenceSpan (int articleNumber,
                        int sectionNumber)
  {
    this.articleNumber = articleNumber;
    this.sectionNumber = sectionNumber;
  }

  @Override
  public void onClick (View view)
  {
    Intent intent = new Intent (view.getContext (),
                                ArticleActivity.class);
    intent.putExtra (ArticleActivity.EXTRA_ARTICLE_NUMBER,
                     articleNumber);
    intent.putExtra (ArticleActivity.EXTRA_MARK_NUMBER,
                     sectionNumber);
    view.getContext ().startActivity (intent);
  }
}
