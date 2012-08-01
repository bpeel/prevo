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
#include "pdb-strcmp.h"
#include "pdb-roman.h"

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

/* A reference represents a position in an article. The reference can
 * either directly contain a pointer to the section and article or it
 * can refer to it indirectly via a mark name. These are used in a
 * PdbDbLink to store the location the link refers to and also as the
 * value of the index entries */
typedef enum
{
  PDB_DB_REFERENCE_TYPE_MARK,
  PDB_DB_REFERENCE_TYPE_DIRECT
} PdbDbReferenceType;

typedef struct
{
  PdbDbReferenceType type;

  union
  {
    struct
    {
      PdbDbArticle *article;
      PdbDbSection *section;
    } direct;

    char *mark;
  } d;
} PdbDbReference;

/* A link stores a delayed reference to a section from a reference
 * span. These are all collected in the 'references' list so that they
 * can be resolved later once all of the articles are loaded */
typedef struct
{
  PdbDbSpan *span;
  PdbDbReference *reference;
} PdbDbLink;

/* PdbDbMark is only used as the value of the 'marks' hash table. This
 * points to a particular secion in an article and is used to convert
 * the mark name to its actual section */
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
  /* Temporary hash table of the translations indexed by language
   * code. This only contains anything while parsing an article */
  GHashTable *translations;

  GHashTable *marks;

  /* This is a list of links. Each link contains a reference to a
   * section (either directly a pointer or a mark name) and and a
   * pointer to the span. The data in the span will be replaced by an
   * article and mark number as a post-processing step once all of the
   * articles have been read so that the references can be resolved.
   * The links are sorted by the offset */
  GList *links;
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
pdb_db_reference_free (PdbDbReference *entry)
{
  switch (entry->type)
    {
    case PDB_DB_REFERENCE_TYPE_MARK:
      g_free (entry->d.mark);
      break;

    case PDB_DB_REFERENCE_TYPE_DIRECT:
      break;
    }

  g_slice_free (PdbDbReference, entry);
}

static PdbDbReference *
pdb_db_reference_copy (const PdbDbReference *entry_in)
{
  PdbDbReference *entry_out = g_slice_dup (PdbDbReference, entry_in);

  switch (entry_in->type)
    {
    case PDB_DB_REFERENCE_TYPE_MARK:
      entry_out->d.mark = g_strdup (entry_out->d.mark);
      break;

    case PDB_DB_REFERENCE_TYPE_DIRECT:
      break;
    }

  return entry_out;
}

static void
pdb_db_link_free (PdbDbLink *link)
{
  pdb_db_reference_free (link->reference);
  g_slice_free (PdbDbLink, link);
}

static void
pdb_db_add_index_entry (PdbDb *db,
                        const char *lang,
                        const char *name,
                        const char *display_name,
                        const PdbDbReference *entry_in)
{
  PdbTrie *trie = pdb_lang_get_trie (db->lang, lang);

  if (trie)
    {
      PdbDbReference *entry =
        pdb_db_reference_copy (entry_in);
      const char *p;

      /* Check if any of the characters in the name are upper case */
      for (p = name; *p; p = g_utf8_next_char (p))
        {
          gunichar ch = g_utf8_get_char (p);

          if (g_unichar_isupper (ch))
            break;
        }

      /* If we found an uppercase character then we'll additionally
       * add a lower case representation of the name so that the
       * search can be case insensitive */
      if (*p || display_name)
        {
          GString *buf = g_string_new (NULL);

          for (p = name; *p; p = g_utf8_next_char (p))
            {
              gunichar ch = g_unichar_tolower (g_utf8_get_char (p));
              g_string_append_unichar (buf, ch);
            }

          pdb_trie_add_word (trie,
                             buf->str,
                             display_name ? display_name : name,
                             entry);

          g_string_free (buf, TRUE);
        }
      else
        {
          pdb_trie_add_word (trie,
                             name,
                             NULL,
                             entry);
        }
    }
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

static int
pdb_db_get_element_num (PdbDocElementNode *element)
{
  PdbDocNode *n;
  int num = 0;

  /* Count the matching elements before this one */
  for (n = element->node.prev; n; n = n->prev)
    if (n->type == PDB_DOC_NODE_TYPE_ELEMENT &&
        !strcmp (((PdbDocElementNode *) n)->name, element->name))
      num++;

  /* If this is the first matching element then check if it's also the
   * only one */
  if (num == 0)
    {
      for (n = element->node.next; n; n = n->next)
        if (n->type == PDB_DOC_NODE_TYPE_ELEMENT &&
            !strcmp (((PdbDocElementNode *) n)->name, element->name))
          /* It's not the only one so return a real number */
          return 0;

      /* It is the only one so return -1 */
      return -1;
    }
  else
    return num;
}

typedef struct
{
  GString *buf;
  GQueue spans;
} PdbDbTranslationData;

static void
pdb_db_free_translation_data_cb (void *user_data)
{
  PdbDbTranslationData *data = user_data;

  if (data->buf)
    g_string_free (data->buf, TRUE);

  pdb_db_free_span_list (data->spans.head);

  g_slice_free (PdbDbTranslationData, data);
}

static int
pdb_db_compare_language_code (const void *a,
                              const void *b,
                              void *user_data)
{
  PdbDb *db = user_data;
  const char *code_a = a;
  const char *code_b = b;
  const char *name_a, *name_b;

  name_a = pdb_lang_get_name (db->lang, code_a);
  if (name_a == NULL)
    name_a = code_a;

  name_b = pdb_lang_get_name (db->lang, code_b);
  if (name_b == NULL)
    name_b = code_b;

  return pdb_strcmp (name_a, name_b);
}

static gboolean
pdb_db_get_trd_link (PdbDb *db,
                     PdbDocElementNode *trd_elem,
                     const PdbDbReference *reference,
                     GString *buf,
                     GQueue *spans,
                     GError **error)
{
  PdbDocElementNode *parent, *kap;
  PdbDocNode *n;
  int sence_num = -1;
  int subsence_num = -1;
  PdbDbSpan *span;
  PdbDbLink *link;
  int span_start, span_end;

  /* Check that the parent is either <snc> or <subsnc> */
  parent = (PdbDocElementNode *) trd_elem->node.parent;

  if (!strcmp (parent->name, "dif"))
    parent = (PdbDocElementNode *) parent->node.parent;

  if (!strcmp (parent->name, "subsnc"))
    {
      subsence_num = pdb_db_get_element_num (parent);
      parent = (PdbDocElementNode *) parent->node.parent;
    }

  if (!strcmp (parent->name, "snc"))
    {
      sence_num = pdb_db_get_element_num (parent);
      parent = (PdbDocElementNode *) parent->node.parent;
    }

  if (!strcmp (parent->name, "subdrv") ||
      !strcmp (parent->name, "subart"))
    parent = (PdbDocElementNode *) parent->node.parent;

  if (strcmp (parent->name, "drv") &&
      strcmp (parent->name, "art"))
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "%s tag found with unknown parent %s",
                   trd_elem->name,
                   parent->name);
      return FALSE;
    }

  kap = pdb_doc_get_child_element (&parent->node, "kap");

  if (kap == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "drv node found without a kap");
      return FALSE;
    }

  span_start = pdb_db_get_utf16_length (buf->str);

  for (n = kap->node.first_child; n; n = n->next)
    {
      switch (n->type)
        {
        case PDB_DOC_NODE_TYPE_TEXT:
          {
            PdbDocTextNode *text = (PdbDocTextNode *) n;
            const char *p, *end;

            for (p = text->data, end = text->data + text->len;
                 p < end;
                 p++)
              if (g_ascii_isspace (*p))
                {
                  if (buf->len > 0 &&
                      !g_ascii_isspace (buf->str[buf->len - 1]))
                    g_string_append_c (buf, ' ');
                }
              else
                g_string_append_c (buf, *p);
          }
          break;

        case PDB_DOC_NODE_TYPE_ELEMENT:
          {
            PdbDocElementNode *elem = (PdbDocElementNode *) n;

            if (!strcmp (elem->name, "tld") ||
                !strcmp (elem->name, "rad"))
              g_string_append_c (buf, '~');
          }
          break;
        }
    }

  if (sence_num != -1)
    {
      g_string_append_printf (buf, " %i", sence_num + 1);

      if (subsence_num != -1)
        g_string_append_printf (buf, ".%c", subsence_num + 'a');
    }

  span_end = pdb_db_get_utf16_length (buf->str);

  span = g_slice_new0 (PdbDbSpan);
  span->span_length = span_end - span_start;
  span->span_start = span_start;
  span->type = PDB_DB_SPAN_REFERENCE;
  g_queue_push_tail (spans, span);

  link = g_slice_new (PdbDbLink);
  link->span = span;
  link->reference = pdb_db_reference_copy (reference);

  db->links = g_list_prepend (db->links, link);

  return TRUE;
}

static gboolean
pdb_db_is_empty_translation (PdbDocElementNode *element)
{
  PdbDocNode *node;

  /* Some of the documents (eg, 'missupozi' for French) have empty
   * translations. We want to totally skip these so that they don't
   * mess up the index */

  for (node = element->node.first_child; node; node = node->next)
    switch (node->type)
      {
      case PDB_DOC_NODE_TYPE_TEXT:
        {
          PdbDocTextNode *text = (PdbDocTextNode *) node;
          const char *end = text->data + text->len;
          const char *p;

          for (p = text->data; p < end; p++)
            if (!g_ascii_isspace (*p))
              return FALSE;
        }
        break;

      case PDB_DOC_NODE_TYPE_ELEMENT:
        if (!pdb_db_is_empty_translation ((PdbDocElementNode *) node))
          return FALSE;
      }

  return TRUE;
}

static gboolean
pdb_db_add_trd_index (PdbDb *db,
                      PdbDocElementNode *element,
                      const char *lang_code,
                      const PdbDbReference *reference,
                      GError **error)
{
  PdbDocElementNode *ind;
  GString *display_name;

  display_name = g_string_new (NULL);

  pdb_doc_append_element_text_with_ignore (element,
                                           display_name,
                                           "ofc",
                                           NULL);
  pdb_db_trim_buf (display_name);

  if ((ind = pdb_doc_get_child_element (&element->node, "ind")))
    {
      GString *real_name = g_string_new (NULL);

      pdb_doc_append_element_text (ind, real_name);
      pdb_db_trim_buf (real_name);

      pdb_db_add_index_entry (db,
                              lang_code,
                              real_name->str,
                              display_name->str,
                              reference);

      g_string_free (real_name, TRUE);
    }
  else if (pdb_doc_element_has_child_element (element))
    {
      GString *real_name = g_string_new (NULL);

      /* We don't want <klr> tags in the index name */
      pdb_doc_append_element_text_with_ignore (element,
                                               real_name,
                                               "ofc",
                                               "klr",
                                               NULL);
      pdb_db_trim_buf (real_name);

      pdb_db_add_index_entry (db,
                              lang_code,
                              real_name->str,
                              display_name->str,
                              reference);

      g_string_free (real_name, TRUE);
    }
  else
    {
      pdb_db_add_index_entry (db,
                              lang_code,
                              display_name->str,
                              NULL,
                              reference);
    }

  g_string_free (display_name, TRUE);

  return TRUE;
}

static gboolean
pdb_db_add_translation_index (PdbDb *db,
                              PdbDocElementNode *element,
                              const PdbDbReference *reference,
                              GError **error)
{
  const char *lang_code;

  lang_code = pdb_doc_get_attribute (element, "lng");

  if (lang_code == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "%s element with no lng attribute",
                   element->name);
      return FALSE;
    }

  if (!strcmp (element->name, "trdgrp"))
    {
      PdbDocNode *node;

      for (node = element->node.first_child; node; node = node->next)
        if (node->type == PDB_DOC_NODE_TYPE_ELEMENT &&
            !pdb_db_add_trd_index (db,
                                   (PdbDocElementNode *) node,
                                   lang_code,
                                   reference,
                                   error))
          return FALSE;

      return TRUE;
    }
  else
    return pdb_db_add_trd_index (db, element, lang_code, reference, error);
}

static gboolean
pdb_db_handle_translation (PdbDb *db,
                           PdbDocElementNode *element,
                           const PdbDbReference *reference,
                           GError **error)
{
  const char *lang_code;
  PdbDbTranslationData *data;
  GString *content;

  /* Silently ignore empty translations */
  if (pdb_db_is_empty_translation (element))
    return TRUE;

  lang_code = pdb_doc_get_attribute (element, "lng");

  if (lang_code == NULL)
    {
      g_set_error (error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "%s element with no lng attribute",
                   element->name);
      return FALSE;
    }

  if ((data = g_hash_table_lookup (db->translations,
                                   lang_code)) == NULL)
    {
      data = g_slice_new (PdbDbTranslationData);
      data->buf = g_string_new (NULL);
      g_queue_init (&data->spans);
      g_hash_table_insert (db->translations,
                           g_strdup (lang_code),
                           data);
    }
  else if (data->buf->len > 0)
    g_string_append (data->buf, "; ");

  if (!pdb_db_get_trd_link (db,
                            element,
                            reference,
                            data->buf,
                            &data->spans,
                            error))
    return FALSE;

  g_string_append (data->buf, ": ");

  content = g_string_new (NULL);
  pdb_doc_append_element_text (element, content);
  pdb_db_trim_buf (content);
  g_string_append_len (data->buf, content->str, content->len);
  g_string_free (content, TRUE);

  return pdb_db_add_translation_index (db,
                                       element,
                                       reference,
                                       error);
}

static gboolean
pdb_db_find_translations_recursive (PdbDb *db,
                                    PdbDocElementNode *root_node,
                                    const PdbDbReference *reference,
                                    GError **error)
{
  GPtrArray *stack;
  gboolean ret = TRUE;

  stack = g_ptr_array_new ();

  if (root_node->node.first_child)
    g_ptr_array_add (stack, root_node->node.first_child);

  while (stack->len > 0)
    {
      PdbDocNode *node = g_ptr_array_index (stack, stack->len - 1);
      g_ptr_array_set_size (stack, stack->len - 1);

      if (node->next)
        g_ptr_array_add (stack, node->next);

      if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
        {
          PdbDocElementNode *element = (PdbDocElementNode *) node;

          if (!strcmp (element->name, "trdgrp") ||
              !strcmp (element->name, "trd"))
            {
              if (!pdb_db_handle_translation (db,
                                              element,
                                              reference,
                                              error))
                {
                  ret = FALSE;
                  break;
                }
            }
          else if (node->first_child &&
                   strcmp (element->name, "ekz") &&
                   strcmp (element->name, "bld") &&
                   strcmp (element->name, "adm") &&
                   strcmp (element->name, "fnt"))
            g_ptr_array_add (stack, node->first_child);
        }
    }

  g_ptr_array_free (stack, TRUE);

  return ret;
}

static gboolean
pdb_db_find_translations (PdbDb *db,
                          PdbDocElementNode *root_node,
                          const PdbDbReference *reference,
                          GError **error)
{
  PdbDocNode *node;
  gboolean ret = TRUE;

  for (node = root_node->node.first_child; node; node = node->next)
    if (node->type == PDB_DOC_NODE_TYPE_ELEMENT)
      {
        PdbDocElementNode *element = (PdbDocElementNode *) node;

        if (!strcmp (element->name, "trdgrp") ||
            !strcmp (element->name, "trd"))
          {
            if (!pdb_db_handle_translation (db,
                                            element,
                                            reference,
                                            error))
              {
                ret = FALSE;
                break;
              }
          }
      }

  return ret;
}

static GList *
pdb_db_flush_translations (PdbDb *db)
{
  GQueue sections;
  GList *keys, *l;

  g_queue_init (&sections);

  keys = g_hash_table_get_keys (db->translations);

  keys = g_list_sort_with_data (keys,
                                pdb_db_compare_language_code,
                                db);
  for (l = keys; l; l = l->next)
    {
      const char *lang_code = l->data;
      const char *lang_name = pdb_lang_get_name (db->lang, lang_code);
      PdbDbTranslationData *data =
        g_hash_table_lookup (db->translations, lang_code);
      PdbDbSection *section = g_slice_new (PdbDbSection);

      if (lang_name == NULL)
        lang_name = lang_code;

      section->title.length = strlen (lang_name);
      section->title.text = g_strdup (lang_name);
      section->title.spans = NULL;

      section->text.length = data->buf->len;
      section->text.text = g_string_free (data->buf, FALSE);
      section->text.spans = data->spans.head;

      data->buf = NULL;
      g_queue_init (&data->spans);

      g_queue_push_tail (&sections, section);
    }

  g_list_free (keys);

  g_hash_table_remove_all (db->translations);

  return sections.head;
}

static void
pdb_db_resolve_links (PdbDb *db)
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

  /* Resolve all of the links */
  for (l = db->links; l; l = l->next)
    {
      PdbDbLink *link = l->data;
      PdbDbReference *ref = link->reference;

      switch (ref->type)
        {
        case PDB_DB_REFERENCE_TYPE_MARK:
          {
            PdbDbMark *mark =
              g_hash_table_lookup (db->marks, ref->d.mark);

            if (mark)
              {
                link->span->data1 = mark->article->article_num;
                link->span->data2 = mark->section->section_num;
              }
            else
              {
                link->span->data1 = 0;
                link->span->data2 = 0;
                fprintf (stderr,
                         "no mark found for reference \"%s\"\n",
                         ref->d.mark);
              }
          }
          break;

        case PDB_DB_REFERENCE_TYPE_DIRECT:
          {
            link->span->data1 = ref->d.direct.article->article_num;
            link->span->data2 = ref->d.direct.section->section_num;
          }
          break;
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
  PdbDbLink *link;
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

  link = g_slice_new (PdbDbLink);
  link->span = span;

  reference = g_slice_new (PdbDbReference);
  reference->type = PDB_DB_REFERENCE_TYPE_MARK;
  reference->d.mark = g_strdup (att[1]);
  link->reference = reference;

  db->links = g_list_prepend (db->links, link);

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
pdb_db_handle_subdrv (PdbDb *db,
                      PdbDbParseState *state,
                      PdbDocElementNode *element,
                      PdbDbSpan *span,
                      GError **error)
{
  int sence_num;

  sence_num = pdb_db_get_element_num (element);

  /* We don't need to do anything if this is the only subdrv */
  if (sence_num != -1)
    {
      pdb_db_start_text (state);
      g_string_append_printf (state->buf, "%c. ", sence_num + 'A');
    }

  return TRUE;
}

static gboolean
pdb_db_handle_snc (PdbDb *db,
                   PdbDbParseState *state,
                   PdbDocElementNode *element,
                   PdbDbSpan *span,
                   GError **error)
{
  int sence_num;

  sence_num = pdb_db_get_element_num (element);

  /* We don't need to do anything if this is the only sence */
  if (sence_num != -1)
    {
      pdb_db_start_text (state);
      g_string_append_printf (state->buf, "%i. ", sence_num + 1);
    }

  return TRUE;
}

static gboolean
pdb_db_handle_subsnc (PdbDb *db,
                      PdbDbParseState *state,
                      PdbDocElementNode *element,
                      PdbDbSpan *span,
                      GError **error)
{
  int sence_num;

  sence_num = pdb_db_get_element_num (element);

  /* We don't need to do anything if this is the only subsence */
  if (sence_num != -1)
    {
      pdb_db_start_text (state);
      g_string_append_printf (state->buf, "%c) ", sence_num + 'a');
    }

  return TRUE;
}

static PdbDbElementSpan
pdb_db_element_spans[] =
  {
    { .name = "ofc", .type = PDB_DB_SPAN_SUPERSCRIPT },
    { .name = "ekz", .type = PDB_DB_SPAN_ITALIC },
    {
      .name = "subdrv",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_subdrv,
      .paragraph = TRUE
    },
    {
      .name = "snc",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_snc,
      .paragraph = TRUE
    },
    {
      .name = "subsnc",
      .type = PDB_DB_SPAN_NONE,
      .handler = pdb_db_handle_subsnc,
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
pdb_db_should_ignore_spannable_tag (PdbDocElementNode *element)
{
  /* Skip citations, adm tags and pictures */
  if (!strcmp (element->name, "fnt") ||
      !strcmp (element->name, "adm") ||
      !strcmp (element->name, "bld") ||
      /* Ignore translations. They are handled separately */
      !strcmp (element->name, "trd") ||
      !strcmp (element->name, "trdgrp"))
    return TRUE;

  /* We want to ignore kap tags within a drv because they are handled
   * separately and parsed into the section header. However, a kap tag
   * can be found within an enclosing kap tag if it is a variation. In
   * that case we do want it to be processed because the section
   * header will be a comma-separated list. To detect this situation
   * we'll just check if any parents are a kap tag */
  if (!strcmp (element->name, "kap"))
    {
      PdbDocElementNode *parent;

      for (parent = (PdbDocElementNode *) element->node.parent;
           parent;
           parent = (PdbDocElementNode *) parent->node.parent)
        if (!strcmp (parent->name, "kap"))
          goto embedded_kap;

      /* If we make it here, the kap is a toplevel kap */
      return TRUE;
    }
 embedded_kap:

  return FALSE;
}

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
        else if (pdb_db_should_ignore_spannable_tag (element))
          {
            /* skip */
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
  const char *display_name, *real_name;
  PdbDocNode *node;
  PdbDbReference entry;

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
          else if (!strcmp (element->name, "var"))
            {
              PdbDocElementNode *child_element;

              /* If the kap contains a variation with an embedded kap
               * then we'll recursively add that to the index too */
              child_element =
                pdb_doc_get_child_element (&element->node, "kap");

              if (child_element)
                pdb_db_add_kap_index (db, child_element, article, section);
            }
        }
        break;
      }

  entry.type = PDB_DB_REFERENCE_TYPE_DIRECT;
  entry.d.direct.article = article;
  entry.d.direct.section = section;

  pdb_db_trim_buf (buf);

  /* If the <kap> tag contains variations then it will be a list with
   * commas in. The variations are ignored in the index text so it
   * will end up as an empty blob of trailing commas. Lets trim
   * these */
  while (buf->len > 0 &&
         (buf->str[buf->len - 1] == ' ' ||
          buf->str[buf->len - 1] == ','))
    g_string_set_size (buf, buf->len - 1);

  if (buf->str[0] == '-' &&
      buf->str[1])
    {
      real_name = buf->str + 1;
      display_name = buf->str;
    }
  else
    {
      real_name = buf->str;
      display_name = NULL;
    }

  pdb_db_add_index_entry (db,
                          "eo", /* language code */
                          real_name,
                          display_name,
                          &entry);

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
  PdbDocElementNode *drv;
  PdbDbSection *section;
  PdbDbReference ref;
  GString *buf;
  int subart_num;

  subart_num = pdb_db_get_element_num (root_node);

  if (subart_num == -1)
    /* It probably doesn't make any sense to have a <subart> on its
     * own, but the article for '-il' seems to do it anyway */
    subart_num = 0;

  section = g_slice_new (PdbDbSection);

  buf = g_string_new (NULL);
  pdb_roman_to_text_append (subart_num + 1, buf);
  g_string_append (buf, ".");
  section->title.length = buf->len;
  section->title.text = g_string_free (buf, FALSE);
  section->title.spans = NULL;

  ref.type = PDB_DB_REFERENCE_TYPE_DIRECT;
  ref.d.direct.article = article;
  ref.d.direct.section = section;

  /* Let's assume the sub-article will either be a collection of
   * <drv>s or directly a spannable string, not a mix */
  if ((drv = pdb_doc_get_child_element (&root_node->node, "drv")))
    {
      /* The first child can optionally be the definition */
      for (node = root_node->node.first_child;
           node && node->type == PDB_DOC_NODE_TYPE_TEXT;
           node = node->next);

      if (node &&
          node->type == PDB_DOC_NODE_TYPE_ELEMENT &&
          !strcmp (((PdbDocElementNode *) node)->name, "dif"))
        {
          if (!pdb_db_parse_spannable_string (db,
                                              (PdbDocElementNode *) node,
                                              &section->text,
                                              error))
            {
              pdb_db_destroy_spannable_string (&section->title);
              g_slice_free (PdbDbSection, section);
              return FALSE;
            }

          if (!pdb_db_find_translations_recursive (db,
                                                   (PdbDocElementNode *) node,
                                                   &ref,
                                                   error))
            return FALSE;

          node = node->next;
        }
      else
        {
          section->text.length = 0;
          section->text.text = g_strdup ("");
          section->text.spans = NULL;
        }

      g_queue_push_tail (sections, section);

      if (!pdb_db_find_translations (db,
                                     root_node,
                                     &ref,
                                     error))
        return FALSE;

      for (; node; node = node->next)
        switch (node->type)
          {
          case PDB_DOC_NODE_TYPE_ELEMENT:
            {
              PdbDocElementNode *element = (PdbDocElementNode *) node;

              if (!strcmp (element->name, "drv"))
                {
                  PdbDbSection *section;

                  section = pdb_db_parse_drv (db, article, element, error);

                  if (section == NULL)
                    return FALSE;
                  else
                    {
                      g_queue_push_tail (sections, section);

                      ref.d.direct.section = section;

                      if (!pdb_db_find_translations_recursive (db,
                                                               element,
                                                               &ref,
                                                               error))
                        return FALSE;
                    }
                }
              else if (strcmp (element->name, "adm") &&
                       strcmp (element->name, "trd") &&
                       strcmp (element->name, "trdgrp") &&
                       /* FIXME - this probably shouldn't strip out
                        * <rim> tags here */
                       strcmp (element->name, "rim"))
                {
                  g_set_error (error,
                               PDB_ERROR,
                               PDB_ERROR_BAD_FORMAT,
                               "<%s> tag found in <subart> that has a <drv>",
                               element->name);
                  return FALSE;
                }
            }
            break;

          case PDB_DOC_NODE_TYPE_TEXT:
            {
              PdbDocTextNode *text = (PdbDocTextNode *) node;
              int i;

              for (i = 0; i < text->len; i++)
                if (!g_ascii_isspace (text->data[i]))
                  {
                    g_set_error (error,
                                 PDB_ERROR,
                                 PDB_ERROR_BAD_FORMAT,
                                 "Unexpected bare text in <subart> that "
                                 "has a <drv>");
                    return FALSE;
                  }
            }
            break;
          }
    }
  else if (!pdb_db_parse_spannable_string (db,
                                           root_node,
                                           &section->text,
                                           error))
    {
      pdb_db_destroy_spannable_string (&section->title);
      g_slice_free (PdbDbSection, section);
      return FALSE;
    }
  else
    {
      g_queue_push_tail (sections, section);

      if (!pdb_db_find_translations_recursive (db,
                                               root_node,
                                               &ref,
                                               error))
        return FALSE;
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
                PdbDbReference ref;

                section = pdb_db_parse_drv (db, article, element, error);

                if (section == NULL)
                  {
                    result = FALSE;
                    break;
                  }

                g_queue_push_tail (&sections, section);

                ref.type = PDB_DB_REFERENCE_TYPE_DIRECT;
                ref.d.direct.article = article;
                ref.d.direct.section = section;

                if (!pdb_db_find_translations_recursive (db,
                                                         element,
                                                         &ref,
                                                         error))
                  {
                    result = FALSE;
                    break;
                  }
              }
            else if (!strcmp (element->name, "subart"))
              {
                if (!pdb_db_parse_subart (db,
                                          article,
                                          element,
                                          &sections,
                                          error))
                  {
                    result = FALSE;
                    break;
                  }
              }
          }

      if (result)
        {
          PdbDbReference ref;

          ref.type = PDB_DB_REFERENCE_TYPE_DIRECT;
          ref.d.direct.article = article;

          if (sections.head == NULL ||
              (ref.d.direct.section = sections.head->data,
               pdb_db_find_translations (db,
                                         root_node,
                                         &ref,
                                         error)))
            {
              GList *translations = pdb_db_flush_translations (db);
              int length = g_list_length (translations);

              if (length > 0)
                {
                  if (sections.head == NULL)
                    sections.head = translations;
                  else
                    {
                      sections.tail->next = translations;
                      translations->prev = sections.tail;
                    }

                  sections.tail = g_list_last (translations);
                  sections.length += length;
                }
            }
          else
            result = FALSE;
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
  pdb_db_reference_free (data);
}

static void
pdb_db_get_reference_cb (void *data,
                         int *article_num,
                         int *mark_num,
                         void *user_data)
{
  PdbDb *db = user_data;
  PdbDbReference *entry = data;

  switch (entry->type)
    {
    case PDB_DB_REFERENCE_TYPE_MARK:
      {
        PdbDbMark *mark = g_hash_table_lookup (db->marks, entry->d.mark);

        if (mark)
          {
            *article_num = mark->article->article_num;
            *mark_num = mark->section->section_num;
          }
        else
          {
            *article_num = 0;
            *mark_num = 0;
            fprintf (stderr,
                     "no mark found for reference \"%s\"\n",
                     entry->d.mark);
          }
      }
      break;

    case PDB_DB_REFERENCE_TYPE_DIRECT:
      *article_num = entry->d.direct.article->article_num;
      *mark_num = entry->d.direct.section->section_num;
      break;
    }
}

static void
pdb_db_add_file_root_mark (PdbDb *db,
                           const char *filename,
                           PdbDbArticle *article)
{
  char *mark_name;

  if (article->sections == NULL)
    return;

  mark_name = g_path_get_basename (filename);

  /* Strip off the extension */
  if (g_str_has_suffix (mark_name, ".xml"))
    mark_name[strlen (mark_name) - 4] = '\0';

  pdb_db_add_mark (db,
                   article,
                   article->sections->data,
                   mark_name);

  g_free (mark_name);
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
  db->links = NULL;

  db->marks = g_hash_table_new_full (g_str_hash,
                                     g_str_equal,
                                     g_free,
                                     (GDestroyNotify) pdb_db_mark_free);

  db->translations = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            (GDestroyNotify) g_free,
                                            pdb_db_free_translation_data_cb);

  files = pdb_revo_list_files (revo, "xml/*.xml", error);

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
              int old_len = db->articles->len;

              parse_result = pdb_db_parse_articles (db,
                                                    pdb_doc_get_root (doc),
                                                    error);
              pdb_doc_free (doc);

              if (parse_result)
                {
                  /* Some articles directly reference the filename
                   * instead of a real mark so we need to add a mark
                   * for each file */
                  if (db->articles->len > old_len)
                    {
                      PdbDbArticle *article =
                        g_ptr_array_index (db->articles, db->articles->len - 1);

                      pdb_db_add_file_root_mark (db,
                                                 file,
                                                 article);
                    }
                }
              else
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
    pdb_db_resolve_links (db);

  return db;
}

static void
pdb_db_free_link_cb (void *ptr,
                     void *user_data)
{
  pdb_db_link_free (ptr);
}

static void
pdb_db_free_link_list (GList *links)
{
  g_list_foreach (links, pdb_db_free_link_cb, NULL);
  g_list_free (links);
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

  pdb_db_free_link_list (db->links);

  g_hash_table_destroy (db->marks);
  g_hash_table_destroy (db->translations);

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
