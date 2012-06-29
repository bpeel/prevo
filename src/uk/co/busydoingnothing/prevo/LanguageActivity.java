package uk.co.busydoingnothing.prevo;

import android.app.ListActivity;
import android.os.Bundle;

public class LanguageActivity extends ListActivity
{
  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setTitle (R.string.select_language);
    setContentView (R.layout.languages);
    setListAdapter (new LanguagesAdapter (this));
  }
}
