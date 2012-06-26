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
  PdbDbArticle *article;
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
pdb_db_handle_article_tag (PdbDb *db,
                           const char *name,
                           const char **atts);

static void
pdb_db_in_article_start_cb (PdbDb *db,
                            const char *name,
                            const char **atts);

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

  PdbDbArticle *next_article;

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
                 PdbDbArticle *article,
                 int mark_num)
{
  PdbDbMark *mark = g_slice_new (PdbDbMark);

  mark->article = article;
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
pdb_db_pop_end_cb (PdbDb *db,
                   const char *name)
{
  if (db->stack->data)
    g_string_append_printf (db->article_buf,
                            "</%s>",
                            (char *) db->stack->data);

  pdb_db_pop (db);
}

static void
pdb_db_append_cd_cb (PdbDb *db,
                     const char *s,
                     int len)
{
  pdb_db_append_data (db, s, len);
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

  pdb_db_handle_article_tag (db, name, atts);
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
      pdb_db_add_index_entry (db,
                              "eo",
                              db->kap_buf->str,
                              db->articles->len,
                              db->stack->mark);
      g_string_append (db->article_buf, "</div>");
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
pdb_db_handle_article_tag (PdbDb *db,
                           const char *name,
                           const char **atts)
{
  const char **att;
  const char *tagname = NULL;
  const char *klass = NULL;
  const char *mark = NULL;
  int flags = db->stack->flags;

  if (!strcmp (name, "tld"))
    {
      pdb_db_push_skip (db);
      g_string_append_len (db->article_buf,
                           db->word_root->str,
                           db->word_root->len);
      return;
    }
  else if (!strcmp (name, "rad"))
    {
      pdb_db_push (db,
                   pdb_db_skip_start_cb,
                   pdb_db_skip_end_cb,
                   pdb_db_root_cd_cb);
      g_string_set_size (db->word_root, 0);
      return;
    }
  else if (!strcmp (name, "fnt") ||
           !strcmp (name, "adm"))
    {
      pdb_db_push_skip (db);
      return;
    }
  else if (!strcmp (name, "trd") || !strcmp (name, "trdgrp"))
    {
      /* TODO: queue translations for later */
      pdb_db_push_skip (db);
      return;
    }
  else if (!strcmp (name, "drv"))
    {
      klass = "drv";
      tagname = "div";
      flags |= PDB_DB_FLAG_IN_DRV;
    }
  else if (!strcmp (name, "kap"))
    {
      if ((flags & PDB_DB_FLAG_IN_DRV) == 0)
        {
          klass = "article-title";
          tagname = "div";
        }
      else
        {
          g_string_append (db->article_buf,
                           "<div class=\"drv-title\">");
          g_string_set_size (db->kap_buf, 0);
          pdb_db_push (db,
                       pdb_db_kap_start_cb,
                       pdb_db_kap_end_cb,
                       pdb_db_kap_cd_cb);
          return;
        }
    }

  /* Any attribute that has a mrk attribute gets converted to a tag
   * with an id attribute and gets added to the mark table */
  for (att = atts; att[0]; att += 2)
    if (!strcmp (att[0], "mrk"))
      {
        mark = att[1];
        break;
      }

  /* We always need a tag if there is a mark */
  if (mark != NULL && tagname == NULL)
    tagname = "span";

  if (tagname != NULL)
    g_string_append_printf (db->article_buf, "<%s", tagname);

  if (mark != NULL)
    {
      pdb_db_add_mark (db,
                       mark,
                       db->next_article,
                       db->article_mark_count);

      g_string_append_printf (db->article_buf,
                              " id=\"mrk%i\"",
                              db->article_mark_count);
    }

  if (klass != NULL)
    g_string_append_printf (db->article_buf, " class=\"%s\"", klass);

  if (tagname)
    g_string_append_c (db->article_buf, '>');

  pdb_db_push (db,
               pdb_db_in_article_start_cb,
               pdb_db_pop_end_cb,
               pdb_db_append_cd_cb);
  db->stack->data = (void *) tagname;
  db->stack->flags = flags;

  if (mark != NULL)
    db->stack->mark = db->article_mark_count++;
}

static void
pdb_db_in_article_start_cb (PdbDb *db,
                            const char *name,
                            const char **atts)
{
  pdb_db_handle_article_tag (db, name, atts);
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
  db->next_article = NULL;
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

          db->next_article = g_slice_new (PdbDbArticle);
          db->article_mark_count = 0;

          pdb_db_push (db,
                       pdb_db_in_article_start_cb,
                       pdb_db_pop_end_cb,
                       pdb_db_append_cd_cb);

          parse_result = pdb_xml_parse (db->parser,
                                        file,
                                        error);

          pdb_db_pop (db);

          g_assert (db->stack == NULL);

          if (parse_result)
            {
              PdbDbArticle *article = db->next_article;

              article->text = g_memdup (db->article_buf->str,
                                        db->article_buf->len + 1);
              article->length = db->article_buf->len;

              g_ptr_array_add (db->articles, article);
            }
          else
            {
              g_slice_free (PdbDbArticle, db->next_article);

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
  char *article_dir;
  gboolean ret = TRUE;

  if (!pdb_try_mkdir (dir, error))
    return FALSE;

  if (!pdb_lang_save (db->lang, dir, error))
    return FALSE;

  article_dir = g_build_filename (dir, "articles", NULL);

  if (pdb_try_mkdir (article_dir, error))
    {
      int i;

      for (i = 0; i < db->articles->len; i++)
        {
          PdbDbArticle *article = g_ptr_array_index (db->articles, i);
          char *article_name = g_strdup_printf ("article-%i.html", i);
          char *full_name = g_build_filename (article_dir, article_name, NULL);
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

  g_free (article_dir);

  return ret;
}
