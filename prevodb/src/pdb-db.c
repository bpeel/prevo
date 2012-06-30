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
#include "pdb-xml.h"
#include "pdb-mkdir.h"

typedef struct
{
  /* An offset into the article buffer where the resolved reference
   * should be stored */
  int offset;
  /* The original name of the reference */
  char *name;
} PdbDbReference;

typedef struct
{
  int length;
  char *text;

  /* This is a list of references. Each reference contains the
   * original reference id from the XML file and an offset into the
   * article buffer. These will replaced by an article and mark number
   * as a post-processing step once all of the articles have been read
   * so that the references can be resolved. The references are sorted
   * by the offset */
  GList *references;
} PdbDbArticle;

typedef struct
{
  int article_num;
  int mark_num;
} PdbDbMark;

typedef struct _PdbDbStackEntry PdbDbStackEntry;

typedef void (* PdbDbStartElementHandler) (PdbDb *db,
                                           const char *name,
                                           const char **atts);
typedef void (* PdbDbEndElementHandler) (PdbDb *db,
                                         const char *name);
typedef void (* PdbDbCharacterDataHandler) (PdbDb *db,
                                            const char *s,
                                            int len);

#define PDB_DB_FLAG_IN_DRV (1 << 0)
#define PDB_DB_FLAG_IN_EKZ (1 << 1)

static void
pdb_db_copy_start_cb (PdbDb *db,
                      const char *name,
                      const char **atts);

static void
pdb_db_copy_end_cb (PdbDb *db,
                    const char *name);

static void
pdb_db_copy_cd_cb (PdbDb *db,
                   const char *s,
                   int len);

struct _PdbDbStackEntry
{
  PdbDbStartElementHandler start_element_handler;
  PdbDbEndElementHandler end_element_handler;
  PdbDbCharacterDataHandler character_data_handler;

  PdbDbStackEntry *next;

  int mark;
  int depth;
  int flags;
  void *data;
};

struct _PdbDb
{
  PdbXmlParser *parser;

  PdbDbStackEntry *stack;

  GError *error;

  PdbLang *lang;

  GQueue references;

  GString *article_buf;

  GPtrArray *articles;

  GHashTable *marks;
  int article_mark_count;

  GString *word_root;
  GString *tmp_buf;
};

static void
pdb_db_add_index_entry (PdbDb *db,
                        const char *lang,
                        const char *name,
                        int article_num,
                        int mark_num)
{
  PdbTrieBuilder *trie = pdb_lang_get_trie (db->lang, lang);

  if (trie)
    {
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
      if (*p)
        {
          GString *buf = g_string_new (NULL);

          for (p = name; *p; p = g_utf8_next_char (p))
            {
              gunichar ch = g_unichar_tolower (g_utf8_get_char (p));
              g_string_append_unichar (buf, ch);
            }

          pdb_trie_builder_add_word (trie,
                                     buf->str,
                                     name,
                                     article_num,
                                     mark_num);

          g_string_free (buf, TRUE);
        }
      else
        {
          pdb_trie_builder_add_word (trie,
                                     name,
                                     NULL,
                                     article_num,
                                     mark_num);
        }
    }
}

static void
pdb_db_append_data (PdbDb *db,
                    const char *s,
                    int len)
{
  while (len > 0)
    {
      const char *next = g_utf8_next_char (s);
      gunichar ch = g_utf8_get_char (s);

      if (ch == '&')
        g_string_append (db->article_buf, "&amp;");
      else if (ch == '<')
        g_string_append (db->article_buf, "&lt;");
      else if (ch == '>')
        g_string_append (db->article_buf, "&glt;");
      else if (ch == '"')
        g_string_append (db->article_buf, "&quot;");
      else if (ch < 128 && g_ascii_isspace (ch))
        {
          if (db->article_buf->len == 0 ||
              db->article_buf->str[db->article_buf->len - 1] != ' ')
            g_string_append_c (db->article_buf, ' ');
        }
      else
        g_string_append_len (db->article_buf, s, next - s);

      len -= next - s;
      s = next;
    }
}

static void
pdb_db_add_mark (PdbDb *db,
                 const char *mark_name,
                 int article_num,
                 int mark_num)
{
  PdbDbMark *mark = g_slice_new (PdbDbMark);

  mark->article_num = article_num;
  mark->mark_num = mark_num;

  g_hash_table_insert (db->marks,
                       g_strdup (mark_name),
                       mark);
}

static void
pdb_db_push (PdbDb *db,
             PdbDbStartElementHandler start_element_handler,
             PdbDbEndElementHandler end_element_handler,
             PdbDbCharacterDataHandler character_data_handler)
{
  PdbDbStackEntry *entry = g_slice_new (PdbDbStackEntry);

  entry->next = db->stack;
  entry->start_element_handler = start_element_handler;
  entry->end_element_handler = end_element_handler;
  entry->character_data_handler = character_data_handler;
  entry->depth = 1;
  entry->data = 0;
  entry->mark = db->stack ? db->stack->mark : -1;
  entry->flags = db->stack ? db->stack->flags : 0;

  db->stack = entry;
}

static void
pdb_db_pop (PdbDb *db)
{
  PdbDbStackEntry *next = db->stack->next;

  g_slice_free (PdbDbStackEntry, db->stack);
  db->stack = next;
}

static void
pdb_db_skip_start_cb (PdbDb *db,
                      const char *name,
                      const char **atts)
{
  db->stack->depth++;
}

static void
pdb_db_skip_end_cb (PdbDb *db,
                    const char *name)
{
  if (--db->stack->depth == 0)
    pdb_db_pop (db);
}

static void
pdb_db_skip_cd_cb (PdbDb *db,
                   const char *s,
                   int len)
{
}

static void
pdb_db_push_skip (PdbDb *db)
{
  pdb_db_push (db,
               pdb_db_skip_start_cb,
               pdb_db_skip_end_cb,
               pdb_db_skip_cd_cb);
}

static void
pdb_db_root_cd_cb (PdbDb *db,
                   const char *s,
                   int len)
{
  int pos = db->article_buf->len;

  pdb_db_append_data (db, s, len);

  g_string_append_len (db->word_root,
                       db->article_buf->str + pos,
                       db->article_buf->len - pos);
}

static void
pdb_db_append_tld (PdbDb *db,
                   GString *buf,
                   const char **atts)
{
  const char *root = db->word_root->str;
  const char **att;

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
pdb_db_kap_start_cb (PdbDb *db,
                     const char *name,
                     const char **atts)
{
  if (!strcmp (name, "tld"))
    pdb_db_append_tld (db, db->tmp_buf, atts);

  pdb_db_copy_start_cb (db, name, atts);
}

static const char *
pdb_db_trim_buf (GString *buf)
{
  const char *start;
  char *end;

  /* Strip trailing spaces from the tmp_buf */
  for (end = buf->str + buf->len;
       end > buf->str && g_ascii_isspace (end[-1]);
       end--);
  *end = '\0';
  /* And leading spaces */
  for (start = buf->str;
       start < end && g_ascii_isspace (*start);
       start++);

  return start;
}

static void
pdb_db_kap_end_cb (PdbDb *db,
                   const char *name)
{
  if (db->stack->mark == -1)
    pdb_xml_abort (db->parser,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "Headword found with no containing mrk");
  else
    {
      const char *start = pdb_db_trim_buf (db->tmp_buf);

      pdb_db_add_index_entry (db,
                              "eo",
                              start,
                              db->articles->len,
                              db->stack->mark);
      g_string_append (db->article_buf, "</kap>");
    }

  pdb_db_pop (db);
}

static void
pdb_db_kap_cd_cb (PdbDb *db,
                  const char *s,
                  int len)
{
  int pos = db->article_buf->len;

  pdb_db_append_data (db, s, len);

  g_string_append_len (db->tmp_buf,
                       db->article_buf->str + pos,
                       db->article_buf->len - pos);
}

static void
pdb_db_translation_start_cb (PdbDb *db,
                             const char *name,
                             const char **atts)
{
  db->stack->depth++;
}

static void
pdb_db_translation_end_cb (PdbDb *db,
                           const char *name)
{
  if (--db->stack->depth <= 0)
    {
      pdb_db_append_data (db, db->tmp_buf->str, db->tmp_buf->len);
      g_string_append (db->article_buf, "</trd>");

      if (db->stack->data)
        {
          char *lang = db->stack->data;
          const char *name;

          name = pdb_db_trim_buf (db->tmp_buf);

          pdb_db_add_index_entry (db,
                                  lang,
                                  name,
                                  db->articles->len,
                                  db->stack->mark);

          g_free (lang);
        }

      pdb_db_pop (db);
    }
}

static void
pdb_db_translation_cd_cb (PdbDb *db,
                          const char *s,
                          int len)
{
  g_string_append_len (db->tmp_buf, s, len);
}

static void
pdb_db_handle_translation (PdbDb *db,
                           const char **atts)
{
  const char **att;
  const char *lang = NULL;

  /* Ignore translations of examples */
  if ((db->stack->flags & PDB_DB_FLAG_IN_EKZ))
    {
      pdb_db_push_skip (db);
      return;
    }

  g_string_set_size (db->tmp_buf, 0);

  for (att = atts; att[0]; att += 2)
    if (!strcmp (att[0], "lng"))
      {
        lang = att[1];
        break;
      }

  pdb_db_push (db,
               pdb_db_translation_start_cb,
               pdb_db_translation_end_cb,
               pdb_db_translation_cd_cb);

  if (lang == NULL)
    pdb_xml_abort (db->parser,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "<trd> tag found with no lng attribute");
  else
    {
      db->stack->data = g_strdup (lang);

      g_string_append (db->article_buf, "<trd lang=\"");
      pdb_db_append_data (db, lang, strlen (lang));
      g_string_append (db->article_buf, "\">");
    }
}

static void
pdb_db_translation_group_start_cb (PdbDb *db,
                                   const char *name,
                                   const char **atts)
{
  if (!strcmp (name, "trd"))
    {
      const char *lang = db->stack->data;

      g_string_set_size (db->tmp_buf, 0);

      pdb_db_push (db,
                   pdb_db_translation_start_cb,
                   pdb_db_translation_end_cb,
                   pdb_db_translation_cd_cb);

      db->stack->data = g_strdup (lang);

      g_string_append (db->article_buf, "<trd>");
    }
  else
    db->stack->depth++;
}

static void
pdb_db_translation_group_end_cb (PdbDb *db,
                                 const char *name)
{
  if (--db->stack->depth <= 0)
    {
      g_string_append (db->article_buf, "</trdgrp>");
      g_free (db->stack->data);
      pdb_db_pop (db);
    }
}

static void
pdb_db_translation_group_cd_cb (PdbDb *db,
                                const char *s,
                                int len)
{
  pdb_db_append_data (db, s, len);
}

static void
pdb_db_handle_translation_group (PdbDb *db,
                                 const char **atts)
{
  const char **att;
  const char *lang = NULL;

  /* Ignore translations of examples */
  if ((db->stack->flags & PDB_DB_FLAG_IN_EKZ))
    {
      pdb_db_push_skip (db);
      return;
    }

  for (att = atts; att[0]; att += 2)
    if (!strcmp (att[0], "lng"))
      {
        lang = att[1];
        break;
      }

  pdb_db_push (db,
               pdb_db_translation_group_start_cb,
               pdb_db_translation_group_end_cb,
               pdb_db_translation_group_cd_cb);

  if (lang == NULL)
    pdb_xml_abort (db->parser,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "<trdgrp> tag found with no lng attribute");
  else
    {
      db->stack->data = g_strdup (lang);

      g_string_append (db->article_buf, "<trdgrp lang=\"");
      pdb_db_append_data (db, lang, strlen (lang));
      g_string_append (db->article_buf, "\">");
    }
}

static void
pdb_db_copy_start_cb (PdbDb *db,
                      const char *name,
                      const char **atts)
{
  int mark = -1;
  int flags = 0;
  const char **att;

  if (!strcmp (name, "rad"))
    {
      pdb_db_push (db,
                   pdb_db_skip_start_cb,
                   pdb_db_skip_end_cb,
                   pdb_db_root_cd_cb);
      g_string_set_size (db->word_root, 0);
      return;
    }
  else if (!strcmp (name, "tld"))
    {
      pdb_db_push_skip (db);
      pdb_db_append_tld (db, db->article_buf, atts);
      return;
    }
  else if (!strcmp (name, "drv"))
    {
      flags |= PDB_DB_FLAG_IN_DRV;
    }
  else if (!strcmp (name, "ekz"))
    {
      flags |= PDB_DB_FLAG_IN_EKZ;
    }
  else if (!strcmp (name, "kap"))
    {
      if ((db->stack->flags & PDB_DB_FLAG_IN_DRV))
        {
          g_string_append (db->article_buf,
                           "<kap>");
          g_string_set_size (db->tmp_buf, 0);
          pdb_db_push (db,
                       pdb_db_kap_start_cb,
                       pdb_db_kap_end_cb,
                       pdb_db_kap_cd_cb);
          return;
        }
    }
  /* Skip citations and adm tags */
  else if (!strcmp (name, "fnt") ||
           !strcmp (name, "adm"))
    {
      pdb_db_push_skip (db);
      return;
    }
  else if (!strcmp (name, "trd"))
    {
      pdb_db_handle_translation (db, atts);
      return;
    }
  else if (!strcmp (name, "trdgrp"))
    {
      pdb_db_handle_translation_group (db, atts);
      return;
    }

  g_string_append_c (db->article_buf, '<');
  g_string_append (db->article_buf, name);

  for (att = atts; att[0]; att += 2)
    {
      if (!strcmp (att[0], "mrk"))
        {
          mark = db->article_mark_count++;
          pdb_db_add_mark (db, att[1], db->articles->len, mark);
          g_string_append_printf (db->article_buf,
                                  " mrk=\"%i\"",
                                  mark);
        }
      else if (!strcmp (att[0], "cel"))
        {
          PdbDbReference *ref = g_slice_new (PdbDbReference);

          g_string_append (db->article_buf, " cel=\"\"");

          /* Queue the reference for later so we can replace with an
           * article and mark number later when all of the articles
           * are available. */
          ref->offset = db->article_buf->len - 1;
          ref->name = g_strdup (att[1]);
          g_queue_push_tail (&db->references, ref);
        }
      else
        {
          g_string_append_c (db->article_buf, ' ');
          g_string_append (db->article_buf, att[0]);
          g_string_append (db->article_buf, "=\"");
          pdb_db_append_data (db, att[1], strlen (att[1]));
          g_string_append_c (db->article_buf, '"');
        }
    }

  g_string_append_c (db->article_buf, '>');

  if (mark == -1 &&
      flags == 0 &&
      db->stack->end_element_handler == pdb_db_copy_end_cb)
    db->stack->depth++;
  else
    {
      pdb_db_push (db,
                   pdb_db_copy_start_cb,
                   pdb_db_copy_end_cb,
                   pdb_db_copy_cd_cb);
      if (mark != -1)
        db->stack->mark = mark;
      db->stack->flags |= flags;
    }
}

static void
pdb_db_copy_end_cb (PdbDb *db,
                    const char *name)
{
  g_string_append (db->article_buf, "</");
  g_string_append (db->article_buf, name);
  g_string_append_c (db->article_buf, '>');

  if (--db->stack->depth <= 0)
    pdb_db_pop (db);
}

static void
pdb_db_copy_cd_cb (PdbDb *db,
                   const char *s,
                   int len)
{
  pdb_db_append_data (db, s, len);
}


static void
pdb_db_start_element_cb (void *user_data,
                         const char *name,
                         const char **atts)
{
  PdbDb *db = user_data;

  db->stack->start_element_handler (db, name, atts);
}

static void
pdb_db_end_element_cb (void *user_data,
                       const char *name)
{
  PdbDb *db = user_data;

  db->stack->end_element_handler (db, name);
}

static void
pdb_db_character_data_cb (void *user_data,
                          const char *s,
                          int len)
{
  PdbDb *db = user_data;

  db->stack->character_data_handler (db, s, len);
}

static void
pdb_db_mark_free (PdbDbMark *mark)
{
  g_slice_free (PdbDbMark, mark);
}

PdbDb *
pdb_db_new (PdbRevo *revo,
            GError **error)
{
  PdbDb *db;
  PdbLang *lang;
  char **files;

  lang = pdb_lang_new (revo, error);

  if (lang == NULL)
    return NULL;

  db = g_slice_new (PdbDb);
  db->lang = lang;

  db->parser = pdb_xml_parser_new (revo);
  db->error = NULL;
  db->article_buf = g_string_new (NULL);
  db->word_root = g_string_new (NULL);
  db->tmp_buf = g_string_new (NULL);
  db->articles = g_ptr_array_new ();
  db->stack = NULL;
  g_queue_init (&db->references);

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
          gboolean parse_result;

          pdb_xml_parser_reset (db->parser);

          pdb_xml_set_user_data (db->parser, db);

          pdb_xml_set_element_handler (db->parser,
                                       pdb_db_start_element_cb,
                                       pdb_db_end_element_cb);
          pdb_xml_set_character_data_handler (db->parser,
                                              pdb_db_character_data_cb);

          g_string_set_size (db->word_root, 0);
          g_string_append_c (db->word_root, '~');

          g_string_set_size (db->article_buf, 0);
          g_string_append (db->article_buf,
                           "<?xml version=\"1.0\"?>\n");

          db->article_mark_count = 0;

          pdb_db_push (db,
                       pdb_db_copy_start_cb,
                       pdb_db_copy_end_cb,
                       pdb_db_copy_cd_cb);

          parse_result = pdb_xml_parse (db->parser,
                                        file,
                                        error);

          pdb_db_pop (db);

          g_assert (db->stack == NULL);

          if (parse_result)
            {
              PdbDbArticle *article = g_slice_new (PdbDbArticle);

              article->text = g_memdup (db->article_buf->str,
                                        db->article_buf->len + 1);
              article->length = db->article_buf->len;
              article->references = db->references.head;
              g_queue_init (&db->references);

              g_ptr_array_add (db->articles, article);
            }
          else
            {
              pdb_db_free (db);
              db = NULL;

              break;
            }
        }

      g_strfreev (files);
    }

  return db;
}

static void
pdb_db_free_reference_cb (void *ptr,
                          void *user_data)
{
  PdbDbReference *ref = ptr;

  g_free (ref->name);
  g_slice_free (PdbDbReference, ref);
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

  pdb_xml_parser_free (db->parser);

  g_string_free (db->word_root, TRUE);
  g_string_free (db->tmp_buf, TRUE);
  g_string_free (db->article_buf, TRUE);

  for (i = 0; i < db->articles->len; i++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, i);

      pdb_db_free_reference_list (article->references);

      g_free (article->text);
    }

  pdb_db_free_reference_list (db->references.head);

  g_ptr_array_free (db->articles, TRUE);

  g_hash_table_destroy (db->marks);

  g_slice_free (PdbDb, db);
}

static gboolean
pdb_db_save_article (PdbDb *db,
                     PdbDbArticle *article,
                     FILE *out,
                     GError **error)
{
  int last_pos = 0;
  GList *l;

  for (l = article->references; l; l = l->next)
    {
      PdbDbReference *ref = l->data;
      PdbDbMark *mark;

      if (fwrite (article->text + last_pos, 1, ref->offset - last_pos, out) !=
          ref->offset - last_pos)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       "%s",
                       strerror (errno));
          return FALSE;
        }

      if ((mark = g_hash_table_lookup (db->marks, ref->name)))
        fprintf (out, "%i:%i", mark->article_num, mark->mark_num);

      last_pos = ref->offset;
    }

  if (fwrite (article->text + last_pos, 1, article->length - last_pos, out) !=
      article->length - last_pos)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s",
                   strerror (errno));
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

  if (pdb_try_mkdir (error, dir, "assets", "articles", error))
    {
      int i;

      for (i = 0; i < db->articles->len; i++)
        {
          PdbDbArticle *article = g_ptr_array_index (db->articles, i);
          char *article_name = g_strdup_printf ("article-%i.xml", i);
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
