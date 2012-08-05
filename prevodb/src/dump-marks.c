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
#include <string.h>

#include "pdb-revo.h"
#include "pdb-xml.h"

static void
add_file_root_mark (GHashTable *hash,
                    const char *filename)
{
  char *mark_name;

  mark_name = g_path_get_basename (filename);

  /* Strip off the extension */
  if (g_str_has_suffix (mark_name, ".xml"))
    mark_name[strlen (mark_name) - 4] = '\0';

  g_hash_table_add (hash, mark_name);
}

static void
get_start_element_handler (void *user_data,
                           const char *name,
                           const char **atts)
{
  GHashTable *hash = user_data;
  const char **att;

  for (att = atts; att[0]; att += 2)
    {
      if (!strcmp (att[0], "mrk"))
        g_hash_table_add (hash, g_strdup (att[1]));
    }
}

static void
get_end_element_handler (void *user_data,
                         const char *name)
{
}

static GHashTable *
get_marks (PdbRevo *revo,
           GError **error)
{
  char **files;
  PdbXmlParser *parser;
  GHashTable *hash;
  char **file_p;

  files = pdb_revo_list_files (revo, "xml/*.xml", error);

  if (files == NULL)
    return NULL;

  hash = g_hash_table_new_full (g_str_hash,
                                g_str_equal,
                                g_free,
                                NULL);

  parser = pdb_xml_parser_new (revo);

  pdb_xml_set_user_data (parser, hash);
  pdb_xml_set_element_handler (parser,
                               get_start_element_handler,
                               get_end_element_handler);

  for (file_p = files; *file_p; file_p++)
    {
      const char *file = *file_p;

      pdb_xml_parser_reset (parser);

      if (!pdb_xml_parse (parser, file, error))
        {
          g_hash_table_destroy (hash);
          hash = NULL;
          break;
        }

      add_file_root_mark (hash, file);
    }

  g_strfreev (files);
  pdb_xml_parser_free (parser);

  return hash;
}

typedef struct
{
  PdbXmlParser *parser;
  GHashTable *hash;
} DumpMarksData;

static void
dump_start_element_handler (void *user_data,
                            const char *name,
                            const char **atts)
{
  DumpMarksData *data = user_data;
  const char **att;

  if (strcmp (name, "ref"))
    return;

  for (att = atts; att[0]; att += 2)
    {
      if (!strcmp (att[0], "cel") &&
          !g_hash_table_lookup_extended (data->hash,
                                         att[1],
                                         NULL,
                                         NULL))
        {
          fprintf (stderr,
                   "%s:%i:%i: missing reference \"%s\"\n",
                   pdb_xml_get_current_filename (data->parser),
                   pdb_xml_get_current_line_number (data->parser),
                   pdb_xml_get_current_column_number (data->parser),
                   att[1]);
        }
    }
}

static void
dump_end_element_handler (void *user_data,
                          const char *name)
{
}

static gboolean
dump_missing_references (PdbRevo *revo,
                         GHashTable *marks,
                         GError **error)
{
  DumpMarksData data;
  gboolean ret = TRUE;
  char **files;
  char **file_p;

  files = pdb_revo_list_files (revo, "xml/*.xml", error);

  if (files == NULL)
    return FALSE;

  data.hash = marks;

  data.parser = pdb_xml_parser_new (revo);

  pdb_xml_set_user_data (data.parser, &data);
  pdb_xml_set_element_handler (data.parser,
                               dump_start_element_handler,
                               dump_end_element_handler);

  for (file_p = files; *file_p; file_p++)
    {
      const char *file = *file_p;

      pdb_xml_parser_reset (data.parser);

      if (!pdb_xml_parse (data.parser, file, error))
        {
          ret = FALSE;
          break;
        }
    }

  g_strfreev (files);

  return ret;
}

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
          GHashTable *marks;

          marks = get_marks (revo, &error);

          if (marks)
            {
              if (!dump_missing_references (revo, marks, &error))
                {
                  fprintf (stderr, "%s\n", error->message);
                  g_clear_error (&error);
                  ret = 1;
                }

              g_hash_table_destroy (marks);
            }
          else
            {
              fprintf (stderr, "%s\n", error->message);
              g_clear_error (&error);
              ret = 1;
            }

          pdb_revo_free (revo);
        }
    }

  return ret;
}
