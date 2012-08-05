/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012 Neil Roberts
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* This list implementation is based on the Wayland source code */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "pdb-list.h"

void
pdb_list_init (PdbList *list)
{
  list->prev = list;
  list->next = list;
}

void
pdb_list_insert (PdbList *list, PdbList *elm)
{
  elm->prev = list;
  elm->next = list->next;
  list->next = elm;
  elm->next->prev = elm;
}

void
pdb_list_remove (PdbList *elm)
{
  elm->prev->next = elm->next;
  elm->next->prev = elm->prev;
  elm->next = NULL;
  elm->prev = NULL;
}

int
pdb_list_length (PdbList *list)
{
  PdbList *e;
  int count;

  count = 0;
  e = list->next;
  while (e != list)
    {
      e = e->next;
      count++;
    }

  return count;
}

int
pdb_list_empty (PdbList *list)
{
  return list->next == list;
}

void
pdb_list_insert_list (PdbList *list,
                      PdbList *other)
{
  if (pdb_list_empty (other))
    return;

  other->next->prev = list;
  other->prev->next = list->next;
  list->next->prev = other->prev;
  list->next = other->next;
}
