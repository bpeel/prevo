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
#include <stdio.h>
#include <errno.h>

#include "pdb-db.h"
#include "pdb-lang.h"
#include "pdb-error.h"
#include "pdb-doc.h"
#include "pdb-mkdir.h"
#include "pdb-error.h"

/* The article file format is a list of strings. Each string comprises of:
 * • A two byte little endian number for the length of the string data
 * • The string data in UTF-8
 * • A list of string spans. These comprise of:
 *   • A two-byte length to mark the end of the span
 *   • A two-byte string offset to mark the start of the span
 *   • Two 16-bit numbers of extra data whose meaning depends on the
 *     span type number above.
 *   • One byte as a number representing the intended formatting of the span.
 *   The list of spans is terminated by two zero bytes (which would otherwise
 *   appear as the start of a zero-length span).
 *
 * The first string represents the title of the article. This would
 * usually be the root of the word.
 *
 * The subsequent strings are in pairs where the first string is the
 * title for the section and the second string is the content. There
 * can be sections with an empty title.
 *
 * The length and string offset are counted in 16-bit units as if the
 * string was encoded in UTF-16.
 */

typedef enum
{
  PDB_DB_SPAN_REFERENCE,
  PDB_DB_SPAN_SUPERSCRIPT,
  PDB_DB_SPAN_ITALIC,
  PDB_DB_SPAN_NOTE,
  PDB_DB_SPAN_BOLD,
  PDB_DB_SPAN_NONE
} PdbDbSpanType;

typedef struct
{
  guint16 span_length;
  guint16 span_start;
  guint16 data1;
  guint16 data2;
  PdbDbSpanType type;
} PdbDbSpan;

typedef struct
{
  /* The span where the reference should be stored */
  PdbDbSpan *span;
  /* The original name of the reference */
  char *name;
} PdbDbReference;

typedef struct
{
  int length;
  char *text;

  /* A list of PdbDbSpans */
  GList *spans;
} PdbDbSpannableString;

typedef struct
{
  int section_num;

  PdbDbSpannableString title;
  PdbDbSpannableString text;
} PdbDbSection;

typedef struct
{
  int article_num;

  PdbDbSpannableString title;

  /* A list of PdbDbSections */
  GList *sections;
} PdbDbArticle;

typedef struct
{
  PdbDbArticle *article;
  PdbDbSection *section;
} PdbDbMark;

struct _PdbDb
{
  PdbLang *lang;

  GPtrArray *articles;

  /* Temporary storage location for the word root. This is only valid
   * while parsing an article */
  char *word_root;

  GHashTable *marks;

  /* This is a list of references. Each reference contains the
   * original reference id from the XML file and a pointer to the
   * span. The data in the span will be replaced by an article and
   * mark number as a post-processing step once all of the articles
   * have been read so that the references can be resolved. The
   * references are sorted by the offset */
  GList *references;
};

typedef struct
{
  const char *name;
  const char *symbol;
} PdbDbRefType;

static const PdbDbRefType
pdb_db_ref_types[] =
  {
    { "vid", "→" },
    { "hom", "→" },
    { "dif", "=" },
    { "sin", "⇒" },
    { "ant", "⇝" },
    { "super", "↗" },
    { "sub", "↘" },
    { "prt", "↘" },
    { "malprt", "↗" },
    { "lst", NULL /* ? */ },
    { "ekz", "●" }
  };

static void
pdb_db_add_index_entry (PdbDb *db,
                        const char *lang,
                        const char *name,
                        PdbDbArticle *article,
                        PdbDbSection *section)
{
  PdbTrie *trie = pdb_lang_get_trie (db->lang, lang);

  if (trie)
    {
      PdbDbMark *mark = g_slice_new (PdbDbMark);
      const char *p;

      /* Check if any of the characters in the name are upper case */
      for (p = name; *p; p = g_utf8_next_char (p))
        {
          gunichar ch = g_utf8_get_char (p);

          if (g_unichar_isupper (ch))
            break;
        }

      mark->article = article;
      mark->section = section;

      /* If we found an uppercase character then we'll additionally
       * add a lower case representation of the name so that the
       * search can be case insensitive */
      if (*p)
        {
          GString *buf = g_string_new (NULL);

          for (p = name; *p; p = g_utf8_next_char (p))
            {
              gunichar ch = g_unichar_tolower (g_utf8_get_char (p));
              g_string_append_unichar (buf, ch);
            }

          pdb_trie_add_word (trie,
                             buf->str,
                             name,
                             mark);

          g_string_free (buf, TRUE);
        }
      else
        {
          pdb_trie_add_word (trie,
                             name,
                             NULL,
                             mark);
        }
    }
}

static void
pdb_db_trim_buf (GString *buf)
{
  char *dst;
  const char *src;

  /* Skip leading spaces and replacing all sets of whitespace
   * characters with a single space */
  for (dst = buf->str, src = buf->str;
       *src;
       src++)
    if (g_ascii_isspace (*src))
      {
        if (dst > buf->str && dst[-1] != ' ')
          *(dst)++ = ' ';
      }
    else
      *(dst++) = *src;

  /* Remove any trailing space */
  if (dst > buf->str && dst[-1] == ' ')
    dst--;

  g_string_set_size (buf, dst - buf->str);
}

static void
pdb_db_append_tld (PdbDb *db,
                   GString *buf,
                   char **atts)
{
  const char *root = db->word_root;
  char **att;

  for (att = atts; att[0]; att += 2)
    if (!strcmp (att[0], "lit"))
      {
        g_string_append (buf, att[1]);

        if (*root)
          root = g_utf8_next_char (root);

        break;
      }

  g_string_append (buf, root);
}

static void
pdb_db_mark_free (PdbDbMark *mark)
{
  g_slice_free (PdbDbMark, mark);
}


static void
pdb_db_reference_free (PdbDbReference *ref)
{
  g_free (ref->name);
  g_slice_free (PdbDbReference, ref);
}

static void
pdb_db_span_free (PdbDbSpan *span)
{
  g_slice_free (PdbDbSpan, span);
}

static void
pdb_db_free_span_list (GList *spans)
{
  g_list_foreach (spans, (GFunc) pdb_db_span_free, NULL);
  g_list_free (spans);
}

static void
pdb_db_destroy_spannable_string (PdbDbSpannableString *string)
{
  g_free (string->text);
  pdb_db_free_span_list (string->spans);
}

static void
pdb_db_free_section_cb (void *ptr,
                        void *user_data)
{
  PdbDbSection *section = ptr;
  pdb_db_destroy_spannable_string (&section->title);
  pdb_db_destroy_spannable_string (&section->text);
  g_slice_free (PdbDbSection, section);
}

static void
pdb_db_free_section_list (GList *sections)
{
  g_list_foreach (sections, pdb_db_free_section_cb, NULL);
  g_list_free (sections);
}

static void
pdb_db_resolve_references (PdbDb *db)
{
  int article_num;
  GList *l;

  /* Calculate the article and section numbers */
  for (article_num = 0; article_num < db->articles->len; article_num++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, article_num);
      int section_num;

      article->article_num = article_num;
      for (section_num = 0, l = article->sections;
           l;
           section_num++, l = l->next)
        {
          PdbDbSection *section = l->data;
          section->section_num = section_num;
        }
    }

  /* Resolve all of the references */
  for (l = db->references; l; l = l->next)
    {
      PdbDbReference *ref = l->data;
      PdbDbMark *mark = g_hash_table_lookup (db->marks, ref->name);

      if (mark)
        {
          ref->span->data1 = mark->article->article_num;
          ref->span->data2 = mark->section->section_num;
        }
      else
        {
          ref->span->data1 = 0;
          ref->span->data2 = 0;
          fprintf (stderr,
                   "no mark found for reference \"%s\"\n",
                   ref->name);
        }
    }
}

typedef enum
{
  PDB_DB_STACK_NODE,
  PDB_DB_STACK_CLOSE_SPAN,
  PDB_DB_STACK_ADD_PARAGRAPH,
  PDB_DB_STACK_CLOSING_CHARACTER
} PdbDbStackType;

typedef struct
{
  PdbDbStackType type;

  union
  {
    PdbDocNode *node;
    PdbDbSpan *span;
    gunichar character;
  } d;
} PdbDbParseStackEntry;

typedef struct
{
  GArray *stack;
  GString *buf;
  GQueue spans;
  gboolean paragraph_queued;
} PdbDbParseState;

static PdbDbParseStackEntry *
pdb_db_parse_push_entry (PdbDbParseState *state,
                         PdbDbStackType type)
{
  PdbDbParseStackEntry *entry;

  g_array_set_size (state->stack, state->stack->len + 1);
  entry = &g_array_index (state->stack,
                          PdbDbParseStackEntry,
                          state->stack->len - 1);

  entry->type = type;

  return entry;
}

static PdbDbParseStackEntry *
pdb_db_parse_push_node (PdbDbParseState *state,
                        PdbDocNode *node)
{
  PdbDbParseStackEntry *entry =
    pdb_db_parse_push_entry (state, PDB_DB_STACK_NODE);

  entry->d.node = node;

  return entry;
}

static PdbDbParseStackEntry *
pdb_db_parse_push_add_paragraph (PdbDbParseState *state)
{
  return pdb_db_parse_push_entry (state, PDB_DB_STACK_ADD_PARAGRAPH);
}

static PdbDbParseStackEntry *
pdb_db_parse_push_closing_character (PdbDbParseState *state,
                                     gunichar ch)
{
  PdbDbParseStackEntry *entry =
    pdb_db_parse_push_entry (state, PDB_DB_STACK_CLOSING_CHARACTER);

  entry->d.character = ch;

  return entry;
}

typedef gboolean (* PdbDbElementHandler) (PdbDb *db,
                                          PdbDbParseState *state,
                                          PdbDocElementNode *element,
                                          PdbDbSpan *span,
                                          GError **error);

typedef struct
{
  const char *name;
  PdbDbSpanType type;
  PdbDbElementHandler handler;
  gboolean paragraph;
} PdbDbElementSpan;

static void
pdb_db_start_text (PdbDbParseState *state)
{
  if (state->paragraph_queued)
    {
      if (state->buf->len > 0)
        g_string_append (state->buf, "\n\n");
      state->paragraph_queued = FALSE;
    }
}

static int
pdb_db_get_utf16_length (const char *buf)
{
  int length = 0;

  /* Calculates the length that the string would have if it was
   * encoded in UTF-16 */
  for (; *buf; buf = g_utf8_next_char (buf))
    {
      gunichar ch = g_utf8_get_char (buf);

      length++;
      /* If the character is outside the BMP then it
       * will need an extra 16 bit number to encode
       * it */
      if (ch >= 0x10000)
        length++;
    }

  return length;
}

static PdbDbSpan *
pdb_db_start_span (PdbDbParseState *state,
                   PdbDbSpanType type)
{
  PdbDbSpan *span = g_slice_new0 (PdbDbSpan);
  PdbDbParseStackEntry *entry;

  span->span_start =
    pdb_db_get_utf16_length (state->buf->str);
  span->type = type;

  g_queue_push_tail (&state->spans, span);
  /* Push the span onto the state.stack so that we can
   * fill in the span length once all of the child
   * nodes have been processed */
  entry = pdb_db_parse_push_entry (state, PDB_DB_STACK_CLOSE_SPAN);
  entry->d.span = span;

  return span;
}

static gboolean
pdb_db_handle_aut (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocElementNode *element,
                   PdbDbSpan *span,
                   GError **error)
{
  pdb_db_start_text (state);
  g_string_append (state->buf, "[");
  pdb_db_parse_push_closing_character (state, ']');

  return TRUE;
}

static gboolean
pdb_db_handle_rim (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocElementNode *element,
                   PdbDbSpan *span,
                   GError **error)
{
  PdbDbSpan *bold_span = g_slice_new0 (PdbDbSpan);

  pdb_db_start_text (state);

  bold_span->type = PDB_DB_SPAN_BOLD;
  bold_span->span_start = pdb_db_get_utf16_length (state->buf->str);

  g_string_append (state->buf, "Rim. ");

  bold_span->span_length = (pdb_db_get_utf16_length (state->buf->str) -
                            bold_span->span_start);
  g_queue_push_tail (&state->spans, bold_span);

  return TRUE;
}

static void
pdb_db_handle_reference_type (PdbDbParseState *state,
                              PdbDocElementNode *element)
{
  const char *parent_name;
  char **att;

  parent_name = ((PdbDocElementNode *) element->node.parent)->name;

  /* Ignore the icon for the reference if the parent is one of the
   * following types. A comment in the code for the WebOS version of
   * PReVo says that the XSLT for ReVo does this too, but I can't seem
   * to find it anymore */
  if (!strcmp (parent_name, "dif") ||
      !strcmp (parent_name, "rim") ||
      !strcmp (parent_name, "ekz") ||
      !strcmp (parent_name, "klr"))
    return;

  for (att = element->atts; att[0]; att += 2)
    if (!strcmp (att[0], "tip"))
      {
        int i;

        for (i = 0; i < G_N_ELEMENTS (pdb_db_ref_types); i++)
          {
            const PdbDbRefType *type = pdb_db_ref_types + i;

            if (!strcmp (att[1], type->name))
              {
                if (type->symbol)
                  {
                    pdb_db_start_text (state);
                    g_string_append (state->buf, type->symbol);
                  }

                break;
              }
          }

        break;
      }
}

static gboolean
pdb_db_handle_ref (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocElementNode *element,
                   PdbDbSpan *span,
                   GError **error)
{
  PdbDbReference *reference;
  char **att;

  for (att = element->atts; att[0]; att += 2)
    if (!strcmp (att[0], "cel"))
      goto found_cel;

  g_set_error (error,
               PDB_ERROR,
               PDB_ERROR_BAD_FORMAT,
               "<ref> tag found with a cel attribute");
  return FALSE;

 found_cel:

  pdb_db_handle_reference_type (state, element);

  span = pdb_db_start_span (state, PDB_DB_SPAN_REFERENCE);

  reference = g_slice_new (PdbDbReference);
  reference->span = span;
  reference->name = g_strdup (att[1]);
  db->references = g_list_prepend (db->references, reference);

  return TRUE;
}

static gboolean
pdb_db_handle_refgrp (PdbDb *db,
                      PdbDbParseState *state,
                      PdbDocElementNode *element,
                      PdbDbSpan *span,
                      GError **error)
{
  pdb_db_handle_reference_type (state, element);

  return TRUE;
}

static gboolean
pdb_db_handle_snc (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocElementNode *element,
                   PdbDbSpan *span,
                   GError **error)
{
  int sence_num = 0;
  PdbDocNode *n;

  /* Count the sncs before this one */
  for (n = element->node.prev; n; n = n->prev)
    if (n->type == PDB_DOC_NODE_TYPE_ELEMENT &&
        !strcmp (((PdbDocElementNode *) n)->name, "snc"))
      sence_num++;

  /* Check if this is the only sence. In that case we don't need to do
   * anything */
  if (sence_num == 0)
    {
      for (n = element->node.next; n; n = n->next)
        if (n->type == PDB_DOC_NODE_TYPE_ELEMENT &&
            !strcmp (((PdbDocElementNode *) n)->name, "snc"))
          goto multiple_sences;

      return TRUE;
    }

 multiple_sences:
  pdb_db_start_text (state);
  g_string_append_printf (state->buf, "%i. ", sence_num + 1);

  return TRUE;
}

static PdbDbElementSpan
pdb_db_element_spans[] =
  {
    { .name = "ofc", .type = PDB_DB_SPAN_SUPERSCRIPT },
    { .name = "ekz", .type = PDB_DB_SPAN_ITALIC },
    {
      .name = "snc",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_snc,
      .paragraph = TRUE
    },
    {
      .name = "ref",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_ref
    },
    {
      .name = "refgrp",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_refgrp
    },
    {
      .name = "rim",
      .type = PDB_DB_SPAN_NOTE,
      .handler = pdb_db_handle_rim,
      .paragraph = TRUE
    },
    { .name = "em", .type = PDB_DB_SPAN_BOLD, },
    { .name = "aut", .type = PDB_DB_SPAN_NONE, .handler = pdb_db_handle_aut },
  };

static gboolean
pdb_db_parse_node (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocNode *node,
                   GError **error)
{
  if (node->next)
    pdb_db_parse_push_node (state, node->next);

  switch (node->type)
    {
    case PDB_DOC_NODE_TYPE_ELEMENT:
      {
        PdbDocElementNode *element = (PdbDocElementNode *) node;

        if (!strcmp (element->name, "tld"))
          {
            pdb_db_start_text (state);
            pdb_db_append_tld (db, state->buf, element->atts);
          }
        /* Skip citations, adm tags and pictures */
        else if (!strcmp (element->name, "fnt") ||
                 !strcmp (element->name, "adm") ||
                 !strcmp (element->name, "bld") ||
                 /* Ignore kap tags. They are parsed separately
                  * into the section headers */
                 !strcmp (element->name, "kap"))
          {
          }
        else if (!strcmp (element->name, "trd"))
          {
            /* FIXME: do something here */
          }
        else if (!strcmp (element->name, "trdgrp"))
          {
            /* FIXME: do something here */
          }
        else if (element->node.first_child)
          {
            int i;

            for (i = 0; i < G_N_ELEMENTS (pdb_db_element_spans); i++)
              {
                const PdbDbElementSpan *elem_span =
                  pdb_db_element_spans + i;

                if (!strcmp (elem_span->name, element->name))
                  {
                    PdbDbSpan *span;

                    if (elem_span->paragraph)
                      {
                        /* Make sure there is a paragraph separator
                         * before and after the remark */
                        state->paragraph_queued = TRUE;
                        pdb_db_parse_push_add_paragraph (state);
                      }

                    if (elem_span->type == PDB_DB_SPAN_NONE)
                      span = NULL;
                    else
                      {
                        pdb_db_start_text (state);
                        span = pdb_db_start_span (state, elem_span->type);
                      }

                    if (elem_span->handler &&
                        !elem_span->handler (db,
                                             state,
                                             element,
                                             span,
                                             error))
                      return FALSE;

                    break;
                  }
              }

            pdb_db_parse_push_node (state,
                                    element->node.first_child);
          }
      }
      break;

    case PDB_DOC_NODE_TYPE_TEXT:
      {
        PdbDocTextNode *text = (PdbDocTextNode *) node;
        const char *p, *end;

        for (p = text->data, end = text->data + text->len;
             p < end;
             p++)
          {
            if (g_ascii_isspace (*p))
              {
                if (state->buf->len > 0 &&
                    state->buf->str[state->buf->len - 1] != ' ' &&
                    state->buf->str[state->buf->len - 1] != '\n')
                  g_string_append_c (state->buf, ' ');
              }
            else
              {
                pdb_db_start_text (state);
                g_string_append_c (state->buf, *p);
              }
          }
      }
      break;
    }

  return TRUE;
}

static gboolean
pdb_db_parse_spannable_string (PdbDb *db,
                               PdbDocElementNode *root_element,
                               PdbDbSpannableString *string,
                               GError **error)
{
  PdbDbParseState state;

  state.stack = g_array_new (FALSE, FALSE, sizeof (PdbDbParseStackEntry));
  state.buf = g_string_new (NULL);
  state.paragraph_queued = FALSE;

  g_queue_init (&state.spans);

  pdb_db_parse_push_node (&state, root_element->node.first_child);

  while (state.stack->len > 0)
    {
      PdbDbParseStackEntry this_entry =
        g_array_index (state.stack, PdbDbParseStackEntry, state.stack->len - 1);

      g_array_set_size (state.stack, state.stack->len - 1);

      switch (this_entry.type)
        {
        case PDB_DB_STACK_CLOSE_SPAN:
          this_entry.d.span->span_length =
            pdb_db_get_utf16_length (state.buf->str) -
            this_entry.d.span->span_start;
          break;

        case PDB_DB_STACK_ADD_PARAGRAPH:
          state.paragraph_queued = TRUE;
          break;

        case PDB_DB_STACK_CLOSING_CHARACTER:
          pdb_db_start_text (&state);
          g_string_append_unichar (state.buf, this_entry.d.character);
          break;

        case PDB_DB_STACK_NODE:
          if (!pdb_db_parse_node (db, &state, this_entry.d.node, error))
            goto error;
          break;
        }
    }

  string->length = state.buf->len;
  string->text = g_string_free (state.buf, FALSE);
  string->spans = state.spans.head;

  g_array_free (state.stack, TRUE);

  return TRUE;

 error:
  g_array_free (state.stack, TRUE);
  g_string_free (state.buf, TRUE);
  pdb_db_free_span_list (state.spans.head);

  return FALSE;
}

static void
pdb_db_add_kap_index (PdbDb *db,
                      PdbDocElementNode *kap,
                      PdbDbArticle *article,
                      PdbDbSection *section)
{
  GString *buf = g_string_new (NULL);
  PdbDocNode *node;

  for (node = kap->node.first_child; node; node = node->next)
    switch (node->type)
      {
      case PDB_DOC_NODE_TYPE_TEXT:
        {
          PdbDocTextNode *text_node = (PdbDocTextNode *) node;
          g_string_append_len (buf, text_node->data, text_node->len);
        }
        break;

      case PDB_DOC_NODE_TYPE_ELEMENT:
        {
          PdbDocElementNode *element = (PdbDocElementNode *) node;

          if (!strcmp (element->name, "tld"))
            pdb_db_append_tld (db, buf, element->atts);
        }
        break;
      }

  pdb_db_trim_buf (buf);
  pdb_db_add_index_entry (db, "eo", buf->str, article, section);

  g_string_free (buf, TRUE);
}

static void
pdb_db_add_mark (PdbDb *db,
                 PdbDbArticle *article,
                 PdbDbSection *section,
                 const char *mark_name)
{
  PdbDbMark *mark = g_slice_new (PdbDbMark);

  mark->article = article;
  mark->section = section;

  g_hash_table_insert (db->marks,
                       g_strdup (mark_name),
                       mark);
}

static void
pdb_db_add_marks (PdbDb *db,
                  PdbDbArticle *article,
                  PdbDbSection *section,
                  PdbDocElementNode *element)
{
  char **att;
  PdbDocNode *node;

  for (att = element->atts; att[0]; att += 2)
    if (!strcmp (att[0], "mrk"))
      {
        pdb_db_add_mark (db, article, section, att[1]);
        break;
      }

  for (node = element->node.first_child; node; node = node->next)
    if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
      pdb_db_add_marks (db, article, section, (PdbDocElementNode *) node);
}

static PdbDbSection *
pdb_db_parse_drv (PdbDb *db,
                  PdbDbArticle *article,
                  PdbDocElementNode *root_node,
                  GError **error)
{
  PdbDocElementNode *kap;
  PdbDbSection *section;
  gboolean result = TRUE;

  kap = pdb_doc_get_child_element (&root_node->node, "kap");

  if (kap == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "<drv> tag found with no <kap>");
      return NULL;
    }

  section = g_slice_new (PdbDbSection);

  pdb_db_add_marks (db, article, section, root_node);

  pdb_db_add_kap_index (db, kap, article, section);

  if (pdb_db_parse_spannable_string (db, kap, &section->title, error))
    {
      if (!pdb_db_parse_spannable_string (db,
                                          root_node,
                                          &section->text,
                                          error))
        result = FALSE;

      if (!result)
        pdb_db_destroy_spannable_string (&section->title);
    }
  else
    result = FALSE;

  if (!result)
    {
      g_slice_free (PdbDbSection, section);
      return NULL;
    }
  else
    return section;
}

static gboolean
pdb_db_parse_subart (PdbDb *db,
                     PdbDbArticle *article,
                     PdbDocElementNode *root_node,
                     GQueue *sections,
                     GError **error)
{
  PdbDocNode *node;

  for (node = root_node->node.first_child; node; node = node->next)
    if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
      {
        PdbDocElementNode *element = (PdbDocElementNode *) node;

        if (!strcmp (element->name, "drv"))
          {
            PdbDbSection *section;

            section = pdb_db_parse_drv (db, article, element, error);

            if (section == NULL)
              return FALSE;
            else
              g_queue_push_tail (sections, section);
          }
      }

  return TRUE;
}

static PdbDbArticle *
pdb_db_parse_article (PdbDb *db,
                      PdbDocElementNode *root_node,
                      GError **error)
{
  PdbDbArticle *article;
  gboolean result = TRUE;
  PdbDocElementNode *kap, *rad;
  GQueue sections;

  kap = pdb_doc_get_child_element (&root_node->node, "kap");

  if (kap == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "<art> tag found with no <kap>");
      return NULL;
    }

  rad = pdb_doc_get_child_element (&kap->node, "rad");

  if (rad == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "<kap> tag found with no <rad>");
      return NULL;
    }

  db->word_root = pdb_doc_get_element_text (rad);
  g_queue_init (&sections);

  article = g_slice_new (PdbDbArticle);

  if (pdb_db_parse_spannable_string (db, kap, &article->title, error))
    {
      PdbDocNode *node;

      for (node = root_node->node.first_child; node; node = node->next)
        if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
          {
            PdbDocElementNode *element = (PdbDocElementNode *) node;

            if (!strcmp (element->name, "drv"))
              {
                PdbDbSection *section;

                section = pdb_db_parse_drv (db, article, element, error);

                if (section == NULL)
                  result = FALSE;
                else
                  g_queue_push_tail (&sections, section);
              }
            else if (!strcmp (element->name, "subart"))
              {
                if (!pdb_db_parse_subart (db,
                                          article,
                                          element,
                                          &sections,
                                          error))
                  result = FALSE;
              }
          }

      if (!result)
        pdb_db_destroy_spannable_string (&article->title);
    }
  else
    result = FALSE;

  g_free (db->word_root);

  if (result)
    {
      article->sections = sections.head;
      return article;
    }
  else
    {
      pdb_db_free_section_list (sections.head);
      g_slice_free (PdbDbArticle, article);
      return NULL;
    }
}

static gboolean
pdb_db_parse_articles (PdbDb *db,
                       PdbDocElementNode *root_node,
                       GError **error)
{
  PdbDocNode *node;

  for (node = root_node->node.first_child; node; node = node->next)
    if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
      {
        PdbDocElementNode *element = (PdbDocElementNode *) node;

        if (!strcmp (element->name, "art"))
          {
            PdbDbArticle *article = pdb_db_parse_article (db, element, error);

            if (article == NULL)
              return FALSE;

            g_ptr_array_add (db->articles, article);
          }
      }

  return TRUE;
}

static void
pdb_db_free_data_cb (void *data,
                     void *user_data)
{
  g_slice_free (PdbDbMark, data);
}

static void
pdb_db_get_reference_cb (void *data,
                         int *article_num,
                         int *mark_num,
                         void *user_data)
{
  PdbDbMark *mark = data;

  *article_num = mark->article->article_num;
  *mark_num = mark->section->section_num;
}

PdbDb *
pdb_db_new (PdbRevo *revo,
            GError **error)
{
  PdbDb *db;
  char **files;

  db = g_slice_new (PdbDb);

  db->lang = pdb_lang_new (revo,
                           pdb_db_free_data_cb,
                           pdb_db_get_reference_cb,
                           db,
                           error);

  if (db->lang == NULL)
    {
      g_slice_free (PdbDb, db);
      return NULL;
    }

  db->articles = g_ptr_array_new ();

  db->marks = g_hash_table_new_full (g_str_hash,
                                     g_str_equal,
                                     g_free,
                                     (GDestroyNotify) pdb_db_mark_free);

  files = pdb_revo_list_files (revo, "revo/xml/*.xml", error);

  if (files == NULL)
    {
      pdb_db_free (db);
      db = NULL;
    }
  else
    {
      char **file_p;

      for (file_p = files; *file_p; file_p++)
        {
          const char *file = *file_p;
          PdbDoc *doc;

          if ((doc = pdb_doc_load (revo, file, error)) == NULL)
            {
              pdb_db_free (db);
              db = NULL;
              break;
            }
          else
            {
              gboolean parse_result;

              parse_result = pdb_db_parse_articles (db,
                                                    pdb_doc_get_root (doc),
                                                    error);
              pdb_doc_free (doc);

              if (!parse_result)
                {
                  pdb_db_free (db);
                  db = NULL;
                  break;
                }
            }
        }

      g_strfreev (files);
    }

  if (db)
    pdb_db_resolve_references (db);

  return db;
}

static void
pdb_db_free_reference_cb (void *ptr,
                          void *user_data)
{
  pdb_db_reference_free (ptr);
}

static void
pdb_db_free_reference_list (GList *references)
{
  g_list_foreach (references, pdb_db_free_reference_cb, NULL);
  g_list_free (references);
}

void
pdb_db_free (PdbDb *db)
{
  int i;

  pdb_lang_free (db->lang);

  for (i = 0; i < db->articles->len; i++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, i);

      pdb_db_destroy_spannable_string (&article->title);
      pdb_db_free_section_list (article->sections);

      g_slice_free (PdbDbArticle, article);
    }

  g_ptr_array_free (db->articles, TRUE);

  pdb_db_free_reference_list (db->references);

  g_hash_table_destroy (db->marks);

  g_slice_free (PdbDb, db);
}

static gboolean
pdb_db_write_string (PdbDb *pdb,
                     const PdbDbSpannableString *string,
                     FILE *out,
                     GError **error)
{
  guint16 len = GUINT16_TO_LE (string->length);
  GList *l;

  fwrite (&len, sizeof (len), 1, out);
  fwrite (string->text, 1, len, out);

  for (l = string->spans; l; l = l->next)
    {
      PdbDbSpan *span = l->data;

      /* Ignore empty spans */
      if (span->span_length > 0)
        {
          guint16 v[4] = { GUINT16_TO_LE (span->span_length),
                           GUINT16_TO_LE (span->span_start),
                           GUINT16_TO_LE (span->data1),
                           GUINT16_TO_LE (span->data2) };
          fwrite (v, sizeof (len), 4, out);
          fputc (span->type, out);
        }
    }

  len = GUINT16_TO_LE (0);
  fwrite (&len, sizeof (len), 1, out);

  return TRUE;
}

static gboolean
pdb_db_save_article (PdbDb *db,
                     PdbDbArticle *article,
                     FILE *out,
                     GError **error)
{
  GList *sl;

  if (!pdb_db_write_string (db, &article->title, out, error))
    return FALSE;

  for (sl = article->sections; sl; sl = sl->next)
    {
      PdbDbSection *section = sl->data;

      if (!pdb_db_write_string (db, &section->title, out, error) ||
          !pdb_db_write_string (db, &section->text, out, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
pdb_db_save (PdbDb *db,
             const char *dir,
             GError **error)
{
  gboolean ret = TRUE;

  if (!pdb_lang_save (db->lang, dir, error))
    return FALSE;

  if (pdb_try_mkdir (error, dir, "assets", "articles", NULL))
    {
      int i;

      for (i = 0; i < db->articles->len; i++)
        {
          PdbDbArticle *article = g_ptr_array_index (db->articles, i);
          char *article_name = g_strdup_printf ("article-%i.bin", i);
          char *full_name = g_build_filename (dir,
                                              "assets",
                                              "articles",
                                              article_name,
                                              NULL);
          gboolean write_status = TRUE;
          FILE *out;

          out = fopen (full_name, "w");

          if (out == NULL)
            {
              g_set_error (error,
                           G_FILE_ERROR,
                           g_file_error_from_errno (errno),
                           "%s: %s",
                           full_name,
                           strerror (errno));
              write_status = FALSE;
            }
          else
            {
              write_status = pdb_db_save_article (db, article, out, error);
              fclose (out);
            }

          g_free (full_name);
          g_free (article_name);

          if (!write_status)
            {
              ret = FALSE;
              break;
            }
        }
    }
  else
    ret = FALSE;

  return ret;
}
