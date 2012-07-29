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
#include "pdb-doc.h"

static gboolean
dump_tree (PdbDocNode *node,
           PdbDocNode *parent,
           int depth,
           GError **error)
{
  int i;

  g_assert (node->parent == parent);

  g_assert (node->prev == NULL || node->prev->next == node);
  g_assert (node->next == NULL || node->next->prev == node);

  for (i = 0; i < depth; i++)
    fputc (' ', stdout);

  if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
    {
      PdbDocElementNode *element = (PdbDocElementNode *) node;
      char **att;

      printf ("<%s", element->name);

      for (att = element->atts; att[0]; att += 2)
        printf (" %s=\"%s\"", att[0], att[1]);
      fputs (">\n", stdout);

      for (node = element->node.first_child; node; node = node->next)
        if (!dump_tree (node, &element->node, depth + 1, error))
          return FALSE;
    }
  else if (node->type == PDB_DOC_NODE_TYPE_TEXT)
    {
      PdbDocTextNode *text = (PdbDocTextNode *) node;
      int i;

      fputc ('"', stdout);
      for (i = 0; i < text->len; i++)
        switch (text->data[i])
          {
          case '\n':
            fputs ("\\n", stdout);
            break;
          case '"':
            fputs ("\\\"", stdout);
            break;
          case '\\':
            fputs ("\\\\", stdout);
            break;
          default:
            fputc (text->data[i], stdout);
            break;
          }
      fputs ("\"\n", stdout);

      g_assert (node->first_child == NULL);
    }
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
test_file (PdbRevo *revo,
           const char *filename,
           GError **error)
{
  PdbDoc *doc = pdb_doc_load (revo, filename, error);
  gboolean ret;

  if (doc)
    {
      ret = dump_tree (&pdb_doc_get_root (doc)->node, NULL, 0, error);

      pdb_doc_free (doc);
    }
  else
    ret = FALSE;

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
          char **files = pdb_revo_list_files (revo, "xml/*.xml", &error);

          if (files == NULL)
            {
              fprintf (stderr, "%s\n", error->message);
              g_clear_error (&error);
              ret = 1;
            }
          else
            {
              char **p;

              for (p = files; *p; p++)
                if (!test_file (revo, *p, &error))
                  {
                    fprintf (stderr, "%s\n", error->message);
                    g_clear_error (&error);
                    ret = 1;
                    break;
                  }

              g_strfreev (files);
            }

          pdb_revo_free (revo);
        }
    }

  return ret;
}
