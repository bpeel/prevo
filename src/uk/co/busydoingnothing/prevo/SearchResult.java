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
}
