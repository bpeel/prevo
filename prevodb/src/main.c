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

#include "config.h"

#include <stdio.h>

#include "pdb-revo.h"
#include "pdb-db.h"

int
main (int argc, char **argv)
{
  GError *error = NULL;
  PdbRevo *revo;
  int ret = 0;

  if (argc != 3)
    {
      fprintf (stderr, "usage: prevodb <revo zip file> <out directory>\n");
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
          PdbDb *db = pdb_db_new (revo, &error);

          if (db == NULL)
            {
              fprintf (stderr, "%s\n", error->message);
              g_clear_error (&error);
              ret = 1;
            }
          else
            {
              if (!pdb_db_save (db, argv[2], &error))
                {
                  fprintf (stderr, "%s\n", error->message);
                  g_clear_error (&error);
                  ret = 1;
                }

              pdb_db_free (db);
            }

          pdb_revo_free (revo);
        }
    }

  return ret;
}
