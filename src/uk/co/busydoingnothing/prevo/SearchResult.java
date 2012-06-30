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

public class SearchResult
{
  String word;
  int article;
  int mark;

  public SearchResult (String word,
                       int article,
                       int mark)
  {
    this.word = word;
    this.article = article;
    this.mark = mark;
  }

  public String getWord ()
  {
    return word;
  }

  public int getArticle ()
  {
    return article;
  }

  public int getMark ()
  {
    return mark;
  }

  @Override
  public String toString ()
  {
    return word;
  }
}
