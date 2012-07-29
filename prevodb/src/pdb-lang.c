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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "pdb-lang.h"
#include "pdb-error.h"
#include "pdb-xml.h"
#include "pdb-strcmp.h"
#include "pdb-trie.h"
#include "pdb-mkdir.h"

typedef struct
{
  char *name;
  char *code;
  PdbTrie *trie;
} PdbLangEntry;

struct _PdbLang
{
  PdbXmlParser *parser;

  GArray *languages;
  GHashTable *hash_table;

  GString *name_buf;
  GString *code_buf;
  gboolean in_lingvo;

  PdbTrieFreeDataCb free_data_cb;
  PdbTrieGetReferenceCb get_reference_cb;
  void *user_data;
};

static void
pdb_lang_start_element_cb (void *user_data,
                           const char *name,
                           const char **atts)
{
  PdbLang *lang = user_data;

  if (lang->in_lingvo)
    {
      pdb_xml_abort (lang->parser,
                     PDB_ERROR,
                     PDB_ERROR_BAD_FORMAT,
                     "Unexpected tag in a ‘lingvo’ tag");
    }
  else if (!strcmp (name, "lingvo"))
    {
      const char *code;
      GError *attrib_error = NULL;

      if (pdb_xml_get_attribute (name, atts, "kodo", &code, &attrib_error))
        {
          g_string_set_size (lang->code_buf, 0);
          g_string_append (lang->code_buf, code);
          g_string_set_size (lang->name_buf, 0);
          lang->in_lingvo = TRUE;
        }
      else
        pdb_xml_abort_error (lang->parser, attrib_error);
    }
}

static void
pdb_lang_end_element_cb (void *user_data,
                         const char *name)
{
  PdbLang *lang = user_data;
  PdbLangEntry *entry;

  lang->in_lingvo = FALSE;

  g_array_set_size (lang->languages, lang->languages->len + 1);
  entry = &g_array_index (lang->languages,
                          PdbLangEntry,
                          lang->languages->len - 1);
  entry->name = g_strdup (lang->name_buf->str);
  entry->code = g_strdup (lang->code_buf->str);
  entry->trie = pdb_trie_new (lang->free_data_cb,
                              lang->get_reference_cb,
                              lang->user_data);
}

static int
pdb_lang_compare_name (const void *a,
                       const void *b)
{
  const PdbLangEntry *ae = a;
  const PdbLangEntry *be = b;

  return pdb_strcmp (ae->name, be->name);
}

static void
pdb_lang_character_data_cb (void *user_data,
                            const char *s,
                            int len)
{
  PdbLang *lang = user_data;

  if (lang->in_lingvo)
    g_string_append_len (lang->name_buf, s, len);
}

static void
pdb_lang_init_hash_table (PdbLang *lang)
{
  int i;

  /* This intialises the hash table with the tries found in the list
   * of languages. The key is the language code and the value is the
   * trie. The key is owned by the entry node */

  for (i = 0; i < lang->languages->len; i++)
    {
      PdbLangEntry *entry = &g_array_index (lang->languages, PdbLangEntry, i);

      g_hash_table_insert (lang->hash_table, entry->code, entry);
    }
}

PdbLang *
pdb_lang_new (PdbRevo *revo,
              PdbTrieFreeDataCb free_data_cb,
              PdbTrieGetReferenceCb get_reference_cb,
              void *user_data,
              GError **error)
{
  PdbLang *lang = g_slice_new (PdbLang);

  lang->languages = g_array_new (FALSE, FALSE, sizeof (PdbLangEntry));
  lang->parser = pdb_xml_parser_new (revo);

  lang->in_lingvo = FALSE;
  lang->name_buf = g_string_new (NULL);
  lang->code_buf = g_string_new (NULL);
  lang->hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  lang->free_data_cb = free_data_cb;
  lang->get_reference_cb = get_reference_cb;
  lang->user_data = user_data;

  pdb_xml_set_user_data (lang->parser, lang);

  pdb_xml_set_element_handler (lang->parser,
                               pdb_lang_start_element_cb,
                               pdb_lang_end_element_cb);
  pdb_xml_set_character_data_handler (lang->parser, pdb_lang_character_data_cb);

  if (pdb_xml_parse (lang->parser,
                     "cfg/lingvoj.xml",
                     error))
    {
      qsort (lang->languages->data,
             lang->languages->len,
             sizeof (PdbLangEntry),
             pdb_lang_compare_name);

      pdb_lang_init_hash_table (lang);
    }
  else
    {
      pdb_lang_free (lang);

      lang = NULL;
    }

  return lang;
}

PdbTrie *
pdb_lang_get_trie (PdbLang *lang,
                   const char *lang_code)
{
  PdbLangEntry *entry;

  entry = g_hash_table_lookup (lang->hash_table, lang_code);

  return entry ? entry->trie : NULL;
}

const char *
pdb_lang_get_name (PdbLang *lang,
                   const char *lang_code)
{
  PdbLangEntry *entry;

  entry = g_hash_table_lookup (lang->hash_table, lang_code);

  return entry ? entry->name : NULL;
}

void
pdb_lang_free (PdbLang *lang)
{
  int i;

  for (i = 0; i < lang->languages->len; i++)
    {
      PdbLangEntry *entry = &g_array_index (lang->languages, PdbLangEntry, i);

      g_free (entry->name);
      g_free (entry->code);
      pdb_trie_free (entry->trie);
    }

  g_array_free (lang->languages, TRUE);

  pdb_xml_parser_free (lang->parser);

  g_string_free (lang->name_buf, TRUE);
  g_string_free (lang->code_buf, TRUE);

  g_hash_table_destroy (lang->hash_table);

  g_slice_free (PdbLang, lang);
}

static void
pdb_lang_write_character_data (const char *str,
                               FILE *out)
{
  while (*str)
    {
      if (*str == '&')
        fputs ("&amp;", out);
      else if (*str == '<')
        fputs ("&lt;", out);
      else if (*str == '>')
        fputs ("&glt;", out);
      else if (*str == '"')
        fputs ("&quot;", out);
      else
        fputc (*str, out);

      str++;
    }
}

static gboolean
pdb_lang_save_language_list (PdbLang *lang,
                             const char *dir,
                             GError **error)
{
  gboolean ret = TRUE;
  char *filename;
  FILE *out;

  if (!pdb_try_mkdir (error, dir, "res", "xml", NULL))
    return FALSE;

  filename = g_build_filename (dir, "res", "xml", "languages.xml", NULL);

  if ((out = fopen (filename, "w")) == NULL)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "%s: %s",
                   filename,
                   strerror (errno));
      ret = FALSE;
    }
  else
    {
      int i;

      fputs ("<?xml version=\"1.0\"?>\n"
             "<languages>\n",
             out);

      for (i = 0; i < lang->languages->len; i++)
        {
          PdbLangEntry *entry =
            &g_array_index (lang->languages, PdbLangEntry, i);

          if (!pdb_trie_is_empty (entry->trie))
            {
              fputs ("<lang code=\"", out);
              pdb_lang_write_character_data (entry->code, out);
              fputs ("\">", out);
              pdb_lang_write_character_data (entry->name, out);
              fputs ("</lang>\n", out);
            }
        }

      fputs ("</languages>\n", out);

      fclose (out);
    }

  g_free (filename);

  return ret;
}

static gboolean
pdb_lang_save_indices (PdbLang *lang,
                       const char *dir,
                       GError **error)
{
  gboolean ret = TRUE;

  if (pdb_try_mkdir (error, dir, "assets", "indices", NULL))
    {
      int i;

      for (i = 0; i < lang->languages->len; i++)
        {
          PdbLangEntry *entry =
            &g_array_index (lang->languages, PdbLangEntry, i);

          if (!pdb_trie_is_empty (entry->trie))
            {
              char *index_name = g_strdup_printf ("index-%s.bin", entry->code);
              char *full_name =
                g_build_filename (dir, "assets", "indices", index_name, NULL);
              guint8 *compressed_data;
              int compressed_len;
              gboolean write_status;

              pdb_trie_compress (entry->trie,
                                 &compressed_data,
                                 &compressed_len);

              write_status = g_file_set_contents (full_name,
                                                  (char *) compressed_data,
                                                  compressed_len,
                                                  error);

              g_free (full_name);
              g_free (index_name);
              g_free (compressed_data);

              if (!write_status)
                {
                  break;
                  ret = FALSE;
                }
            }
        }
    }
  else
    ret = FALSE;

  return ret;
}

gboolean
pdb_lang_save (PdbLang *lang,
               const char *dir,
               GError **error)
{
  if (!pdb_lang_save_language_list (lang, dir, error))
    return FALSE;

  if (!pdb_lang_save_indices (lang, dir, error))
    return FALSE;

  return TRUE;
}
