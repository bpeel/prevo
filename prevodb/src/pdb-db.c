#include "config.h"

#include <string.h>

#include "pdb-db.h"
#include "pdb-lang.h"
#include "pdb-error.h"
#include "pdb-xml.h"

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

struct _PdbDbStackEntry
{
  PdbDbStartElementHandler start_element_handler;
  PdbDbEndElementHandler end_element_handler;
  PdbDbCharacterDataHandler character_data_handler;

  PdbDbStackEntry *next;

  int depth;
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
};

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
pdb_db_pop_end_cb (PdbDb *db,
                   const char *name)
{
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
pdb_db_in_article_start_cb (PdbDb *db,
                            const char *name,
                            const char **atts)
{
  const char **att;
  const char *tagname;

  tagname = "span";

  g_string_append_printf (db->article_buf, "<%s", tagname);

  /* Any attribute that has a mrk attribute gets converted to a span
   * with an id tag and gets added to the mark table */
  for (att = atts; att[0]; att += 2)
    if (!strcmp (att[0], "mrk"))
      {
        pdb_db_add_mark (db,
                         att[1],
                         db->next_article,
                         db->article_mark_count);

        g_string_append_printf (db->article_buf,
                                " id=\"mrk%i\"",
                                db->article_mark_count);

        db->article_mark_count++;

        break;
      }

  g_string_append_c (db->article_buf, '>');

  pdb_db_push (db,
               pdb_db_in_article_start_cb,
               pdb_db_pop_end_cb,
               pdb_db_append_cd_cb);
  db->stack->data = (void *) tagname;
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

              g_print ("%s", article->text);

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
