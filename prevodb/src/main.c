#include "config.h"

#include <stdio.h>

#include "pdb-revo.h"
#include "pdb-lang.h"

int
main (int argc, char **argv)
{
  GError *error = NULL;
  PdbRevo *revo;
  int ret = 0;

  if (argc != 2)
    {
      fprintf (stderr, "usage: prevodb <revo zip file>\n");
      ret = 1;
    }
  else
    {
      revo = pdb_revo_new (argv[1], &error);

      if (revo == NULL)
        {
          fprintf (stderr, "%s\n", error->message);
          g_clear_error (&error);
          ret = 1;
        }
      else
        {
          PdbLang *lang = pdb_lang_new (revo, &error);

          if (lang == NULL)
            {
              fprintf (stderr, "%s\n", error->message);
              g_clear_error (&error);
              ret = 1;
            }
          else
            {
              pdb_lang_free (lang);
            }

          pdb_revo_free (revo);
        }
    }

  return ret;
}
