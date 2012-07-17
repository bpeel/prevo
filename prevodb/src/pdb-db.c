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
 */

typedef enum
{
  PDB_DB_SPAN_REFERENCE
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

  /* This is a list of references. Each reference contains the
   * original reference id from the XML file and a pointer to the
   * span. The data in the span will be replaced by an article and
   * mark number as a post-processing step once all of the articles
   * have been read so that the references can be resolved. The
   * references are sorted by the offset */
  GList *references;
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
pdb_db_free_span_cb (void *ptr,
                     void *user_data)
{
  PdbDbSpan *span = ptr;
  g_slice_free (PdbDbSpan, span);
}

static void
pdb_db_free_span_list (GList *spans)
{
  g_list_foreach (spans, pdb_db_free_span_cb, NULL);
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

  /* Calculate the article and section numbers */
  for (article_num = 0; article_num < db->articles->len; article_num++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, article_num);
      GList *l;
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
  for (article_num = 0; article_num < db->articles->len; article_num++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, article_num);
      GList *rl, *next;

      for (rl = article->references; rl; rl = next)
        {
          PdbDbReference *ref = rl->data;
          PdbDbMark *mark = g_hash_table_lookup (db->marks, ref->name);

          next = rl->next;

          if (mark)
            {
              ref->span->data1 = mark->article->article_num;
              ref->span->data2 = mark->section->section_num;
            }
          else
            {
              fprintf (stderr,
                       "no mark found for reference \"%s\"\n",
                       ref->name);
              pdb_db_reference_free (ref);
              article->references =
                g_list_delete_link (article->references, rl);
            }
        }
    }
}

typedef struct
{
  PdbDocNode *node;
} PdbDbParseStackEntry;

static PdbDbParseStackEntry *
pdb_doc_parse_push_node (GArray *stack,
                         PdbDocNode *node)
{
  PdbDbParseStackEntry *entry;

  g_array_set_size (stack, stack->len + 1);
  entry = &g_array_index (stack, PdbDbParseStackEntry, stack->len - 1);

  entry->node = node;

  return entry;
}

static gboolean
pdb_doc_parse_spannable_string (PdbDb *db,
                                PdbDocElementNode *root_element,
                                PdbDbSpannableString *string,
                                GError **error)
{
  GArray *stack = g_array_new (FALSE, FALSE, sizeof (PdbDbParseStackEntry));
  GString *buf = g_string_new (NULL);
  GQueue spans;

  g_queue_init (&spans);

  pdb_doc_parse_push_node (stack, root_element->node.first_child);

  while (stack->len > 0)
    {
      PdbDbParseStackEntry this_entry =
        g_array_index (stack, PdbDbParseStackEntry, stack->len - 1);

      g_array_set_size (stack, stack->len - 1);

      if (this_entry.node->next)
        pdb_doc_parse_push_node (stack, this_entry.node->next);

      switch (this_entry.node->type)
        {
        case PDB_DOC_NODE_TYPE_ELEMENT:
          {
            PdbDocElementNode *element = (PdbDocElementNode *) this_entry.node;

            if (!strcmp (element->name, "tld"))
              pdb_db_append_tld (db, buf, element->atts);
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
              pdb_doc_parse_push_node (stack, element->node.first_child);
          }
          break;

        case PDB_DOC_NODE_TYPE_TEXT:
          {
            PdbDocTextNode *text = (PdbDocTextNode *) this_entry.node;
            const char *p, *end;

            for (p = text->data, end = text->data + text->len;
                 p < end;
                 p++)
              {
                if (g_ascii_isspace (*p))
                  {
                    if (buf->len > 0 && buf->str[buf->len - 1] != ' ')
                      g_string_append_c (buf, ' ');
                  }
                else
                  g_string_append_c (buf, *p);
              }
          }
          break;
        }
    }

  string->length = buf->len;
  string->text = g_string_free (buf, FALSE);
  string->spans = spans.head;

  g_array_free (stack, TRUE);

  return TRUE;
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

  pdb_db_add_kap_index (db, kap, article, section);

  if (pdb_doc_parse_spannable_string (db, kap, &section->title, error))
    {
      if (!pdb_doc_parse_spannable_string (db,
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

  if (pdb_doc_parse_spannable_string (db, kap, &article->title, error))
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
      pdb_db_free_reference_list (article->references);

      g_slice_free (PdbDbArticle, article);
    }

  g_ptr_array_free (db->articles, TRUE);

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
      guint16 v[4] = { GUINT16_TO_LE (span->span_length),
                       GUINT16_TO_LE (span->span_start),
                       GUINT16_TO_LE (span->data1),
                       GUINT16_TO_LE (span->data2) };
      fwrite (v, sizeof (len), 4, out);
      fputc (span->type, out);
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
