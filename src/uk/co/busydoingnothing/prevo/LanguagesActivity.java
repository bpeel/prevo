package uk.co.busydoingnothing.prevo;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

public class LanguagesActivity extends ListActivity
{
  @Override
  public void onCreate (Bundle savedInstanceState)
  {
    super.onCreate (savedInstanceState);
    setTitle (R.string.select_language);
    setContentView (R.layout.languages);
    setListAdapter (new LanguagesAdapter (this));

    ListView lv = getListView ();

    lv.setTextFilterEnabled (true);

    lv.setOnItemClickListener (new AdapterView.OnItemClickListener ()
      {
        public void onItemClick (AdapterView<?> parent,
                                 View view,
                                 int position,
                                 long id)
        {
          LanguagesAdapter adapter =
            (LanguagesAdapter) parent.getAdapter ();
          Object item = adapter.getItem (position);

          if (item instanceof Language)
            {
              Language lang = (Language) item;
              Intent intent = new Intent (view.getContext (),
                                          SearchActivity.class);
              intent.putExtra (SearchActivity.EXTRA_LANGUAGE,
                               lang.getCode ());
              startActivity (intent);
            }
        }
      });
  }
}
