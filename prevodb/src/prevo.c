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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "pdb-revo.h"
#include "pdb-db.h"
#include "pdb-file.h"
#include "pdb-groff.h"

#define PREVO_ERROR (prevo_error_quark ())

#define MAX_LANGUAGE_CODE_SIZE 3
#define LANGUAGE_ENTRY_SIZE (sizeof (guint32) + MAX_LANGUAGE_CODE_SIZE + 1)

typedef enum
{
  PREVO_ERROR_INVALID_FORMAT,
  PREVO_ERROR_NO_SUCH_LANGUAGE,
  PREVO_ERROR_NO_SUCH_ARTICLE
} PrevoError;

static const char prevo_magic[4] = "PRDB";

static const char *option_db_file = NULL;
static const char *option_arg0 = NULL;
static const char *option_arg1 = NULL;
static gboolean option_complete = FALSE;

static GOptionEntry
options[] =
  {
    {
      "db", 'd', 0, G_OPTION_ARG_STRING, &option_db_file,
      "Location of the database file", NULL
    },
    {
      "complete", 'c', 0, G_OPTION_ARG_NONE, &option_complete,
      "Show completions for the word or language instead of an article", NULL
    },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
  };

static GQuark
prevo_error_quark (void)
{
  return g_quark_from_static_string ("prevo-error-quark");
}

static gboolean
process_arguments (int *argc, char ***argv,
                   GError **error)
{
  GOptionContext *context;
  gboolean ret;
  GOptionGroup *group;

  group = g_option_group_new (NULL, /* name */
                              NULL, /* description */
                              NULL, /* help_description */
                              NULL, /* user_data */
                              NULL /* destroy notify */);
  g_option_group_add_entries (group, options);
  context = g_option_context_new ("[language] <word>");
  g_option_context_set_main_group (context, group);
  ret = g_option_context_parse (context, argc, argv, error);
  g_option_context_free (context);

  if (ret)
    {
      if (*argc > 3)
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
                       "Unknown option '%s'", (* argv)[3]);
          ret = FALSE;
        }
      else if (*argc == 2)
        {
          option_arg0 = (* argv)[1];
          option_arg1 = NULL;
        }
      else if (*argc == 3)
        {
          option_arg0 = (* argv)[1];
          option_arg1 = (* argv)[2];
        }
      else
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                       "A word to lookup must be specified. See --help");
          ret = FALSE;
        }
    }

  return ret;
}

static char *
find_db_file (void)
{
  const char * const *system_data_dirs;
  const char * const *dir;

  if (option_db_file)
    return g_strdup (option_db_file);

  system_data_dirs = g_get_system_data_dirs ();

  for (dir = system_data_dirs; *dir; dir++)
    {
      char *full_name = g_build_filename (*dir, "prevo", "prevo.db", NULL);

      if (g_file_test (full_name, G_FILE_TEST_IS_REGULAR))
        return full_name;

      g_free (full_name);
    }

  return NULL;
}

typedef struct
{
  void *data_ptr;
  void *map_ptr;
  size_t map_size;
} MapData;

static gboolean
map_region (PdbFile *file,
            size_t length,
            off_t offset,
            MapData *map_data,
            GError **error)
{
  size_t page_size = (size_t) sysconf (_SC_PAGESIZE);
  off_t inset = offset & (page_size - 1);
  off_t map_start = offset & ~(page_size - 1);

  map_data->map_size = length + inset;

  map_data->map_ptr = mmap (NULL, /* address */
                            map_data->map_size,
                            PROT_READ,
                            MAP_PRIVATE,
                            fileno (file->file),
                            map_start);

  if (map_data->map_ptr == (void *) -1)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s", file->filename, strerror (errno));
      return FALSE;
    }

  map_data->data_ptr = (char *) map_data->map_ptr + inset;

  return TRUE;
}

static void
unmap_region (MapData *map_data)
{
  munmap (map_data->map_ptr, map_data->map_size);
}

static gboolean
map_language_table (PdbFile *file,
                    guint32 *n_languages,
                    MapData *map_data,
                    GError **error)
{
  guint32 n_articles;

  if (!pdb_file_read_32 (file, &n_articles, error) ||
      !pdb_file_seek (file, n_articles * sizeof (guint32), SEEK_CUR, error) ||
      !pdb_file_read_32 (file, n_languages, error))
    return FALSE;

  return map_region (file,
                     *n_languages * LANGUAGE_ENTRY_SIZE,
                     file->pos,
                     map_data,
                     error);
}

static int
search_language_table (const guint8 *language_table,
                       guint32 n_languages,
                       const char *language)
{
  int language_len = strlen (language);
  int begin = 0, end = n_languages;
  const guint8 *pos;

  if (language_len > MAX_LANGUAGE_CODE_SIZE)
    return -1;

  while (end > begin)
    {
      int mid = (end + begin) / 2;
      int comp;

      pos = language_table + mid * LANGUAGE_ENTRY_SIZE;
      comp = strncmp (language, (const char *) pos, MAX_LANGUAGE_CODE_SIZE);

      if (comp < 0)
        end = mid;
      else if (comp > 0)
        begin = mid + 1;
      else
        {
          begin = mid;
          break;
        }
    }

  pos = language_table + begin * LANGUAGE_ENTRY_SIZE;

  if (begin < n_languages && !memcmp (pos, language, language_len))
    return begin;
  else
    return -1;
}

static gboolean
map_language_trie (PdbFile *file,
                   const char *language,
                   MapData *map_data,
                   GError **error)
{
  guint32 n_languages;
  const guint8 *language_table;
  int search_index;
  int trie_offset;
  guint32 trie_size;
  guint8 ch;

  if (!map_language_table (file, &n_languages, map_data, error))
    return FALSE;

  language_table = map_data->data_ptr;

  search_index = search_language_table (language_table, n_languages, language);

  if (search_index != -1 &&
      !strcmp ((const char *) language_table +
               search_index * LANGUAGE_ENTRY_SIZE,
               language))
    {
      guint32 val = *(guint32 *) (language_table +
                                  search_index * LANGUAGE_ENTRY_SIZE +
                                  MAX_LANGUAGE_CODE_SIZE + 1);
      trie_offset = GUINT32_FROM_LE (val);
    }
  else
    trie_offset = -1;

  unmap_region (map_data);

  if (trie_offset == -1)
    {
      g_set_error (error,
                   PREVO_ERROR,
                   PREVO_ERROR_NO_SUCH_LANGUAGE,
                   "The language “%s” was not found in the database",
                   language);
      return FALSE;
    }

  if (!pdb_file_seek (file, trie_offset, SEEK_SET, error))
    return FALSE;

  /* Skip the null-terminated language name */
  do
    if (!pdb_file_read_8 (file, &ch, error))
      return FALSE;
  while (ch);

  if (!pdb_file_read_32 (file, &trie_size, error))
    return FALSE;

  trie_size = GUINT32_FROM_LE (trie_size) & 0x7fffffff;

  if (trie_size < sizeof (guint32))
    {
      g_set_error (error,
                   PREVO_ERROR,
                   PREVO_ERROR_INVALID_FORMAT,
                   "%s: Invalid trie size", file->filename);
      return FALSE;
    }

  return map_region (file,
                     trie_size,
                     file->pos - sizeof (guint32),
                     map_data,
                     error);
}

static gboolean
complete_language (PdbFile *file,
                   const char *language,
                   GError **error)
{
  guint32 n_languages;
  const guint8 *language_table;
  MapData map_data;
  int search_index;
  gboolean ret = TRUE;

  if (!map_language_table (file, &n_languages, &map_data, error))
    return FALSE;

  language_table = map_data.data_ptr;

  search_index = search_language_table (language_table, n_languages, language);

  if (search_index != -1)
    {
      while (search_index < n_languages)
        {
          const guint8 *pos = (language_table +
                               search_index++ * LANGUAGE_ENTRY_SIZE);

          if (!memchr (pos, '\0', MAX_LANGUAGE_CODE_SIZE + 1))
            {
              g_set_error (error,
                           PREVO_ERROR,
                           PREVO_ERROR_INVALID_FORMAT,
                           "%s: A language code is missing the null terminator",
                           file->filename);
              ret = FALSE;
              break;
            }

          if (!g_str_has_prefix ((const char *) pos, language))
            break;

          printf ("%s\n", pos);
        }
    }

  unmap_region (&map_data);

  return ret;
}

static guint32
get_uint32 (const void *ptr)
{
  guint32 ret;

  /* The pointer isn't necessarily aligned so we can't get away with
   * just casting it directly */
  memcpy (&ret, ptr, sizeof (ret));

  return GUINT32_FROM_LE (ret);
}

static guint16
get_uint16 (const void *ptr)
{
  guint16 ret;

  /* The pointer isn't necessarily aligned so we can't get away with
   * just casting it directly */
  memcpy (&ret, ptr, sizeof (ret));

  return GUINT16_FROM_LE (ret);
}

static const guint8 *
search_trie (const guint8 *trie_start,
             const char *search_term)
{
  while (*search_term)
    {
      int character_len = g_utf8_next_char (search_term) - search_term;
      const guint8 *child_start, *trie_end;
      guint32 trie_length;
      gboolean valid_entry;

      /* Get the total length of this node */
      trie_length = get_uint32 (trie_start);

      if ((trie_length & 0x80000000))
        {
          valid_entry = TRUE;
          trie_length &= 0x7fffffff;
        }
      else
        valid_entry = FALSE;

      if (trie_length < 5)
        return NULL;

      /* Skip the character for this node */
      child_start = (const guint8 *) g_utf8_next_char (trie_start + 4);

      /* If this is a valid entry then it is followed by the matching
       * articles which we want to skip */
      if (valid_entry)
        {
          gboolean has_next, has_display_name;

          do
            {
              if (child_start - trie_start + 3 > trie_length)
                return NULL;

              has_next = (child_start[1] & 0x80) != 0;
              has_display_name = (child_start[1] & 0x40) != 0;

              child_start += 3;

              if (has_display_name)
                {
                  if (child_start - trie_start >= trie_length)
                    return NULL;
                  child_start += *child_start + 1;
                }
            } while (has_next);
        }

      trie_end = trie_start + trie_length;
      trie_start = child_start;

      /* trie_start is now pointing into the children of the
       * selected node. We'll scan over these until we either find a
       * matching character for the next character of the prefix or
       * we hit the end of the node */
      while (TRUE)
        {
          int node_ch_len;

          /* If we've reached the end of the node then we haven't
           * found a matching character for the prefix so there are
           * no results */
          if (trie_start + 5 > trie_end)
            return NULL;

          node_ch_len = (g_utf8_next_char (trie_start + 4) -
                         (const char *) trie_start - 4);
          if (trie_start + 4 + node_ch_len > trie_end)
            return NULL;

          /* If we've found a matching character then start scanning
           * into this node */
          if (node_ch_len == character_len &&
              !memcmp (trie_start + 4, search_term, character_len))
            break;
          /* Otherwise skip past the node to the next sibling */
          else
            trie_start += get_uint32 (trie_start) & 0x7fffffff;
        }

      search_term += character_len;
    }

  /* trie_start is now pointing at the last node with this string.
   * Any children of that node are therefore extensions of the
   * prefix. */
  return trie_start;
}

typedef struct
{
  const guint8 *search_start;
  const guint8 *search_end;
  int string_length;
} TrieStack;

static void
trie_stack_push (GArray *stack,
                 const guint8 *search_start,
                 const guint8 *search_end,
                 int string_length)
{
  TrieStack *entry;

  g_array_set_size (stack, stack->len + 1);
  entry = &g_array_index (stack, TrieStack, stack->len - 1);
  entry->search_start = search_start;
  entry->search_end = search_end;
  entry->string_length = string_length;
}

static const TrieStack *
trie_stack_get_top (GArray *stack)
{
  return &g_array_index (stack, TrieStack, stack->len - 1);
}

static void
trie_stack_pop (GArray *stack)
{
  g_array_set_size (stack, stack->len - 1);
}

static void
show_matches (const guint8 *trie_start,
              const char *prefix)
{
  GString *string_buf = g_string_new (prefix);
  GArray *stack = g_array_new (FALSE, FALSE, sizeof (TrieStack));
  gboolean first_char = TRUE;

  /* trie_start points to the last node which matches the end of the
   * prefix. We can now depth-first search the tree to get all the
   * results in sorted order */

  trie_stack_push (stack,
                   trie_start,
                   trie_start + (get_uint32 (trie_start) & 0x7fffffff),
                   string_buf->len);

  while (stack->len > 0)
    {
      const TrieStack *stack_top = trie_stack_get_top (stack);
      const guint8 *search_start = stack_top->search_start;
      const guint8 *search_end = stack_top->search_end;
      guint32 offset;
      int character_len;
      const guint8 *children_start;
      int old_length;

      g_string_set_size (string_buf, stack_top->string_length);

      trie_stack_pop (stack);

      if (search_end - search_start < sizeof (guint32) + 1)
        break;

      offset = get_uint32 (search_start);
      character_len = (g_utf8_next_char (search_start + 4) -
                       (const char *) search_start - 4);
      children_start = search_start + 4 + character_len;
      old_length = string_buf->len;

      if (search_end - search_start < sizeof (guint32) + character_len)
        break;

      if (first_char)
        first_char = FALSE;
      else
        g_string_append_len (string_buf,
                             (const char *) search_start + 4,
                             character_len);

      /* If this is a complete word then display it */
      if ((offset & 0x80000000))
        {
          gboolean has_next;

          fwrite (string_buf->str, 1, string_buf->len, stdout);
          fputc ('\n', stdout);

          do
            {
              guint16 article;
              gboolean has_display_name;

              if (children_start + 3 > search_end)
                goto done;

              article = get_uint16 (children_start);
              has_next = (article & 0x8000) != 0;
              has_display_name = (article & 0x4000) != 0;

              children_start += 3;

              if (has_display_name)
                {
                  if (children_start + 1 > search_end)
                    goto done;

                  children_start += *children_start + 1;
                }
            }
          while (has_next);

          offset &= 0x7fffffff;
        }

      /* If there is a sibling then make sure we continue from that
       * after we've descended through the children of this node */
      if (search_start + offset < search_end)
        trie_stack_push (stack, search_start + offset, search_end, old_length);

      /* Push a search for the children of this node */
      if (children_start < search_start + offset)
        trie_stack_push (stack,
                         children_start,
                         search_start + offset,
                         string_buf->len);
    }

 done:
  g_string_free (string_buf, TRUE);
  g_array_free (stack, TRUE);
}

static char *
get_search_term (const char *language,
                 const char *word)
{
  /* FIXME: Make this handle x-notation when the language is "eo" */
  return g_utf8_strdown (word, -1);
}

static gboolean
complete_word (PdbFile *file,
               const char *language,
               const char *word,
               GError **error)
{
  MapData map_data;
  const guint8 *trie_start;
  char *search_term;

  if (!map_language_trie (file, language, &map_data, error))
    return FALSE;

  trie_start = map_data.data_ptr;

  search_term = get_search_term (language, word);
  trie_start = search_trie (trie_start, search_term);

  if (trie_start)
    show_matches (trie_start, search_term);

  g_free (search_term);

  unmap_region (&map_data);

  return TRUE;
}

static gboolean
show_spanned_string (PdbFile *file,
                     FILE *out,
                     GError **error)
{
  char *string_data;
  guint16 string_len;
  guint16 span_length;

  if (!pdb_file_read_16 (file, &string_len, error))
    return FALSE;

  string_data = g_alloca (string_len);

  if (!pdb_file_read (file, string_data, string_len, error))
    return FALSE;

  fwrite (string_data, 1, string_len, out);

  while (TRUE)
    {
      if (!pdb_file_read_16 (file, &span_length, error))
        return FALSE;

      if (span_length == 0)
        return TRUE;

      if (!pdb_file_seek (file, sizeof (guint16) * 3 + 1, SEEK_CUR, error))
        return FALSE;
    }
}

static gboolean
show_article (PdbFile *file,
              int article_num,
              int mark_num,
              GError **error)
{
  guint32 n_articles;
  guint32 article_offset;
  guint32 article_size;
  size_t article_end;
  PdbGroff *groff;
  FILE *out;
  gboolean ret = TRUE;

  if (!pdb_file_seek (file, 4, SEEK_SET, error) ||
      !pdb_file_read_32 (file, &n_articles, error))
    return FALSE;

  if (article_num < 0 || article_num >= n_articles)
    {
      g_set_error (error,
                   PREVO_ERROR,
                   PREVO_ERROR_INVALID_FORMAT,
                   "Index points to an invalid article number %i",
                   article_num);
      return FALSE;
    }

  if (!pdb_file_seek (file, article_num * 4, SEEK_CUR, error) ||
      !pdb_file_read_32 (file, &article_offset, error) ||
      !pdb_file_seek (file, article_offset, SEEK_SET, error) ||
      !pdb_file_read_32 (file, &article_size, error))
    return FALSE;

  article_end = file->pos + article_size;

  groff = pdb_groff_new (error);

  if (groff == NULL)
    return FALSE;

  out = pdb_groff_get_output (groff);

  while (file->pos < article_end)
    if (show_spanned_string (file, out, error))
      fputs ("\n\n", out);
    else
      {
        ret = FALSE;
        break;
      }

  if (ret)
    {
      if (!pdb_groff_display (groff, error))
        ret = FALSE;
    }

  pdb_groff_free (groff);

  return ret;
}

static gboolean
search_article (PdbFile *file,
                const char *language,
                const char *word,
                GError **error)
{
  MapData map_data;
  const guint8 *trie_start;
  char *search_term;
  int article_num = -1;
  int mark_num = -1;

  if (!map_language_trie (file, language, &map_data, error))
    return FALSE;

  trie_start = map_data.data_ptr;

  search_term = get_search_term (language, word);
  trie_start = search_trie (trie_start, search_term);

  if (trie_start)
    {
      guint32 trie_length = get_uint32 (trie_start);

      if ((trie_length & 0x80000000))
        {
          const guint8 *data_start;

          trie_length &= 0x7fffffff;

          if (trie_length >= 5 &&
              (data_start =
               (const guint8 *) g_utf8_next_char (trie_start + 4)) -
              trie_start + 3 <= trie_length)
            {
              article_num = get_uint16 (data_start) & 0x3fff;
              mark_num = data_start[2];
            }
        }
    }

  g_free (search_term);

  unmap_region (&map_data);

  if (article_num != -1)
    return show_article (file, article_num, mark_num, error);
  else
    {
      g_set_error (error,
                   PREVO_ERROR,
                   PREVO_ERROR_NO_SUCH_ARTICLE,
                   "No article found for “%s”",
                   word);
      return FALSE;
    }
}

static gboolean
process_file (const char *db_filename,
              GError **error)
{
  PdbFile file;
  gboolean ret = TRUE;

  if (pdb_file_open (&file, db_filename, PDB_FILE_MODE_READ, error))
    {
      char magic[sizeof (prevo_magic)];

      if (pdb_file_read (&file, magic, sizeof (magic), error))
        {
          const char *language;
          const char *word;

          if (option_arg1)
            {
              language = option_arg0;
              word = option_arg1;
            }
          else
            {
              language = "eo";
              word = option_arg0;
            }

          if (memcmp (magic, prevo_magic, sizeof (magic)))
            {
              g_set_error (error,
                           PREVO_ERROR,
                           PREVO_ERROR_INVALID_FORMAT,
                           "%s is not a PReVo database",
                           db_filename);
              ret = FALSE;
            }
          else if (option_complete)
            {
              if (option_arg1)
                {
                  if (!complete_word (&file,
                                      language,
                                      word,
                                      error))
                    ret = FALSE;
                }
              else if (!complete_language (&file, option_arg0, error))
                ret = FALSE;
            }
          else if (!search_article (&file, language, word, error))
            ret = FALSE;
        }
      else
        ret = FALSE;

      if (!pdb_file_close (&file, ret ? error : NULL))
        ret = FALSE;
    }
  else
    ret = FALSE;

  return ret;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  int ret = 0;

  if (!process_arguments (&argc, &argv, &error))
    {
      fprintf (stderr, "%s\n", error->message);
      ret = 1;
    }
  else
    {
      char *db_filename;

      db_filename = find_db_file ();

      if (db_filename == NULL)
        {
          fprintf (stderr,
                   "No database file found. You can specify directly it "
                   "with the -d option\n");
          ret = 1;
        }
      else
        {
          if (!process_file (db_filename, &error))
            {
              fprintf (stderr, "%s\n", error->message);
              g_clear_error (&error);
              ret = 1;
            }

          g_free (db_filename);
        }
    }

  return ret;
}
