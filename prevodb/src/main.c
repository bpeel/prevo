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

static gboolean option_single = FALSE;
static const char *option_in_file = NULL;
static const char *option_out_file = NULL;

static GOptionEntry
options[] =
  {
    {
      "single", 's', 0, G_OPTION_ARG_NONE, &option_single,
      "Generate a single file instead of a db for Android", NULL
    },
    {
      "in", 'i', 0, G_OPTION_ARG_STRING, &option_in_file,
      "The zip file or directory containing the ReVo XML files", NULL
    },
    {
      "out", 'o', 0, G_OPTION_ARG_STRING, &option_out_file,
      "Location for the output of the database", NULL
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

static gboolean
process_arguments (int *argc, char ***argv,
                   GError **error)
{
  GOptionContext *context;
  gboolean ret;
  GOptionGroup *group;

  group = g_option_group_new (NULL, /* name */
                              NULL, /* description */
                              NULL, /* help_description */
                              NULL, /* user_data */
                              NULL /* destroy notify */);
  g_option_group_add_entries (group, options);
  context = g_option_context_new ("- Creates a compact database from the "
                                  "ReVo XML files");
  g_option_context_set_main_group (context, group);
  ret = g_option_context_parse (context, argc, argv, error);
  g_option_context_free (context);

  if (ret)
    {
      if (*argc > 1)
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                       "Unknown option '%s'", (* argv)[1]);
          ret = FALSE;
        }
      else if (option_in_file == NULL || option_out_file == NULL)
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                       "The -i and -o options are required. See --help");
          ret = FALSE;
        }
    }

  return ret;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  PdbRevo *revo;
  int ret = 0;

  if (!process_arguments (&argc, &argv, &error))
    {
      fprintf (stderr, "%s\n", error->message);
      ret = 1;
    }
  else
    {
      revo = pdb_revo_new (option_in_file, &error);

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
              gboolean save_ret;

              if (option_single)
                save_ret = pdb_db_save_single (db, option_out_file, &error);
              else
                save_ret = pdb_db_save (db, option_out_file, &error);

              if (!save_ret)
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
