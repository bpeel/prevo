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

#include "pdb-db.h"
#include "pdb-lang.h"
#include "pdb-error.h"
#include "pdb-xml.h"
#include "pdb-mkdir.h"

typedef struct
{
  int length;
  char *text;
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

  GString *article_buf;

  GPtrArray *articles;

  GHashTable *marks;
  int article_mark_count;

  GString *word_root;
  GString *kap_buf;
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
    pdb_trie_builder_add_word (trie, name, article_num, mark_num);
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
pdb_db_kap_start_cb (PdbDb *db,
                     const char *name,
                     const char **atts)
{
  if (!strcmp (name, "tld"))
    g_string_append_len (db->kap_buf,
                         db->word_root->str,
                         db->word_root->len);

  pdb_db_copy_start_cb (db, name, atts);
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
      int end;
      const char *start;

      /* Strip trailing spaces from the kap_buf */
      for (end = db->kap_buf->len;
           end > 0 && g_ascii_isspace (db->kap_buf->str[end]);
           end--);
      db->kap_buf->str[end] = '\0';
      /* And leading spaces */
      for (start = db->kap_buf->str;
           *start && g_ascii_isspace (*start);
           start++);

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

  g_string_append_len (db->kap_buf,
                       db->article_buf->str + pos,
                       db->article_buf->len - pos);
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
      pdb_db_append_data (db, db->word_root->str, db->word_root->len);
      return;
    }
  else if (!strcmp (name, "drv"))
    {
      flags |= PDB_DB_FLAG_IN_DRV;
    }
  else if (!strcmp (name, "kap"))
    {
      if ((db->stack->flags & PDB_DB_FLAG_IN_DRV))
        {
          g_string_append (db->article_buf,
                           "<kap>");
          g_string_set_size (db->kap_buf, 0);
          pdb_db_push (db,
                       pdb_db_kap_start_cb,
                       pdb_db_kap_end_cb,
                       pdb_db_kap_cd_cb);
          return;
        }
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
  db->kap_buf = g_string_new (NULL);
  db->articles = g_ptr_array_new ();
  db->stack = NULL;

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

void
pdb_db_free (PdbDb *db)
{
  int i;

  pdb_lang_free (db->lang);

  pdb_xml_parser_free (db->parser);

  g_string_free (db->word_root, TRUE);
  g_string_free (db->kap_buf, TRUE);
  g_string_free (db->article_buf, TRUE);

  for (i = 0; i < db->articles->len; i++)
    {
      PdbDbArticle *article = g_ptr_array_index (db->articles, i);

      g_free (article->text);
    }

  g_ptr_array_free (db->articles, TRUE);

  g_hash_table_destroy (db->marks);

  g_slice_free (PdbDb, db);
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
          gboolean write_status;

          write_status = g_file_set_contents (full_name,
                                              article->text,
                                              article->length,
                                              error);

          g_free (full_name);
          g_free (article_name);

          if (!write_status)
            {
              break;
              ret = FALSE;
            }
        }
    }
  else
    ret = FALSE;

  return ret;
}
