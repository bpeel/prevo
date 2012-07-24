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

#ifndef PDB_DOC_H
#define PDB_DOC_H

#include <glib.h>

#include "pdb-revo.h"

typedef struct _PdbDoc PdbDoc;
typedef struct _PdbDocNode PdbDocNode;

typedef enum
{
  PDB_DOC_NODE_TYPE_TEXT,
  PDB_DOC_NODE_TYPE_ELEMENT
} PdbDocNodeType;

struct _PdbDocNode
{
  PdbDocNodeType type;
  PdbDocNode *prev, *next;
  PdbDocNode *parent;
  PdbDocNode *first_child;
};

typedef struct
{
  PdbDocNode node;

  int len;
  char data[1];
} PdbDocTextNode;

typedef struct
{
  PdbDocNode node;

  char *name;
  char *atts[1];
} PdbDocElementNode;

PdbDoc *
pdb_doc_load (PdbRevo *revo,
              const char *filename,
              GError **error);

void
pdb_doc_free (PdbDoc *doc);

PdbDocElementNode *
pdb_doc_get_root (PdbDoc *doc);

char *
pdb_doc_get_element_text (PdbDocElementNode *node);

void
pdb_doc_append_element_text (PdbDocElementNode *node,
                             GString *buf);

PdbDocElementNode *
pdb_doc_get_child_element (PdbDocNode *node,
                           const char *tag_name);

#endif /* PDB_DOC_H */
