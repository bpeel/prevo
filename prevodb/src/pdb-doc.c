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

#include <string.h>

#include "pdb-doc.h"
#include "pdb-error.h"
#include "pdb-xml.h"

#ifdef __GNUC__
#define alignof(x) __alignof__ (x)
#else
#error "FIXME: add an implementation of the alignof macro for this compiler"
#endif

typedef struct _PdbDocSlab PdbDocSlab;

/* All of the allocations are made out of slabs of 2kb. That way the
 * whole document can be freed by just freeing the few slabs instead
 * of having to walk the entire tree */
struct _PdbDocSlab
{
  PdbDocSlab *next;
};

#define PDB_DOC_SLAB_SIZE 2048

struct _PdbDoc
{
  PdbDocSlab *slabs;
  PdbDocElementNode *root_node;
};

typedef struct
{
  PdbDocNode *node;
  PdbDocNode *last_child;
} PdbDocStackEntry;

typedef struct
{
  PdbDoc *doc;
  PdbXmlParser *parser;
  GArray *element_stack;
  size_t slab_used;
} PdbDocData;

static size_t
pdb_doc_align (size_t base,
               int alignment)
{
  return (base + alignment - 1) & ~(alignment - 1);
}

static void *
pdb_doc_allocate (PdbDocData *data,
                  size_t size,
                  int alignment)
{
  PdbDocSlab *slab;
  size_t offset;

  offset = pdb_doc_align (data->slab_used, alignment);

  if (size + offset > PDB_DOC_SLAB_SIZE)
    {
      /* Start a new slab */
      slab = g_malloc (PDB_DOC_SLAB_SIZE);
      slab->next = data->doc->slabs;
      data->doc->slabs = slab;

      offset = pdb_doc_align (sizeof (PdbDocSlab), alignment);
    }
  else
    slab = data->doc->slabs;

  data->slab_used = offset + size;

  return (guint8 *) slab + offset;
}

static char *
pdb_doc_strdup (PdbDocData *data,
                const char *str)
{
  int len = strlen (str);
  char *ptr = pdb_doc_allocate (data, len + 1, 1 /* alignment */);

  memcpy (ptr, str, len + 1);

  return ptr;
}

static void
pdb_doc_append_node (PdbDocData *data,
                     PdbDocNode *node)
{
  node->next = NULL;
  node->first_child = NULL;

  if (data->element_stack->len == 0)
    {
      node->prev = NULL;
      node->parent = NULL;
    }
  else
    {
      PdbDocStackEntry *entry;

      entry = &g_array_index (data->element_stack,
                              PdbDocStackEntry,
                              data->element_stack->len - 1);

      node->prev = entry->last_child;

      if (entry->last_child)
        entry->last_child->next = node;
      else
        entry->node->first_child = node;

      entry->last_child = node;
      node->parent = entry->node;
    }
}

static void
pdb_doc_start_element_handler (void *user_data,
                               const char *name,
                               const char **atts)
{
  PdbDocData *data = user_data;
  PdbDocElementNode *node;
  PdbDocStackEntry *entry;
  int n_atts;
  const char **att;

  for (att = atts, n_atts = 0; att[0]; att += 2, n_atts++);

  node = pdb_doc_allocate (data,
                           sizeof (PdbDocElementNode) +
                           sizeof (char *) * n_atts * 2,
                           alignof (PdbDocElementNode));
  node->node.type = PDB_DOC_NODE_TYPE_ELEMENT;
  node->name = pdb_doc_strdup (data, name);

  for (att = atts, n_atts = 0; att[0]; att += 2, n_atts++)
    {
      node->atts[n_atts * 2] = pdb_doc_strdup (data, att[0]);
      node->atts[n_atts * 2 + 1] = pdb_doc_strdup (data, att[1]);
    }

  node->atts[n_atts * 2] = NULL;

  pdb_doc_append_node (data, &node->node);

  if (data->element_stack->len == 0)
    data->doc->root_node = node;

  g_array_set_size (data->element_stack, data->element_stack->len + 1);
  entry = &g_array_index (data->element_stack,
                          PdbDocStackEntry,
                          data->element_stack->len - 1);
  entry->node = &node->node;
  entry->last_child = NULL;
}

static void
pdb_doc_end_element_handler (void *user_data,
                             const char *name)
{
  PdbDocData *data = user_data;

  g_array_set_size (data->element_stack, data->element_stack->len - 1);
}


static void
pdb_doc_character_data_handler (void *user_data,
                                const char *s,
                                int len)
{
  PdbDocData *data = user_data;
  PdbDocTextNode *node;
  PdbDocStackEntry *entry;

  if (data->element_stack->len == 0)
    return;

  entry = &g_array_index (data->element_stack,
                          PdbDocStackEntry,
                          data->element_stack->len - 1);
  /* Check if we can just concatenate this text directly on to a
   * previous text node */
  if (entry->last_child &&
      entry->last_child->type == PDB_DOC_NODE_TYPE_TEXT &&
      data->slab_used + len <= PDB_DOC_SLAB_SIZE)
    {
      node = (PdbDocTextNode *) entry->last_child;
      memcpy (node->data + node->len, s, len);
      node->len += len;
      data->slab_used += len;
    }
  else
    {
      node = pdb_doc_allocate (data,
                               G_STRUCT_OFFSET (PdbDocTextNode, data) + len,
                               alignof (PdbDocTextNode));
      node->node.type = PDB_DOC_NODE_TYPE_TEXT;
      node->len = len;
      memcpy (node->data, s, len);

      pdb_doc_append_node (data, &node->node);
    }
}

PdbDoc *
pdb_doc_load (PdbRevo *revo,
              const char *filename,
              GError **error)
{
  PdbDocData data;
  gboolean ret;

  data.parser = pdb_xml_parser_new (revo);
  data.doc = g_slice_new0 (PdbDoc);
  data.slab_used = PDB_DOC_SLAB_SIZE;
  data.element_stack = g_array_new (FALSE, FALSE, sizeof (PdbDocStackEntry));

  pdb_xml_set_character_data_handler (data.parser,
                                      pdb_doc_character_data_handler);
  pdb_xml_set_element_handler (data.parser,
                               pdb_doc_start_element_handler,
                               pdb_doc_end_element_handler);
  pdb_xml_set_user_data (data.parser, &data);

  ret = pdb_xml_parse (data.parser, filename, error);

  pdb_xml_parser_free (data.parser);
  g_array_free (data.element_stack, TRUE);

  if (ret)
    return data.doc;
  else
    {
      pdb_doc_free (data.doc);
      return NULL;
    }
}

void
pdb_doc_free (PdbDoc *doc)
{
  PdbDocSlab *slab, *next;

  for (slab = doc->slabs; slab; slab = next)
    {
      next = slab->next;
      g_free (slab);
    }

  g_slice_free (PdbDoc, doc);
}

PdbDocElementNode *
pdb_doc_get_root (PdbDoc *doc)
{
  return doc->root_node;
}

void
pdb_doc_append_element_text (PdbDocElementNode *element,
                             GString *buf)
{
  if (element->node.first_child)
    {
      GPtrArray *stack = g_ptr_array_new ();

      g_ptr_array_add (stack, element->node.first_child);

      while (stack->len > 0)
        {
          PdbDocNode *node = g_ptr_array_index (stack, stack->len - 1);
          g_ptr_array_set_size (stack, stack->len - 1);

          if (node->next)
            g_ptr_array_add (stack, node->next);

          if (node->first_child)
            g_ptr_array_add (stack, node->first_child);

          if (node->type == PDB_DOC_NODE_TYPE_TEXT)
            {
              PdbDocTextNode *text_node = (PdbDocTextNode *) node;

              g_string_append_len (buf, text_node->data, text_node->len);
            }
        }

      g_ptr_array_free (stack, TRUE);
    }
}

char *
pdb_doc_get_element_text (PdbDocElementNode *node)
{
  GString *buf = g_string_new (NULL);

  pdb_doc_append_element_text (node, buf);

  return g_string_free (buf, FALSE);
}

PdbDocElementNode *
pdb_doc_get_child_element (PdbDocNode *node,
                           const char *tag_name)
{
  PdbDocElementNode *element;

  for (node = node->first_child; node; node = node->next)
    if (node->type == PDB_DOC_NODE_TYPE_ELEMENT &&
        !strcmp ((element = (PdbDocElementNode *) node)->name, tag_name))
      return element;

  return NULL;
}
