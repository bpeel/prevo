#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "pdb-lang.h"
#include "pdb-error.h"
#include "pdb-xml.h"
#include "pdb-strcmp.h"
#include "pdb-trie.h"

typedef struct
{
  char *name;
  char *code;
  PdbTrieBuilder *trie;
} PdbLangEntry;

struct _PdbLang
{
  PdbXmlParser *parser;

  GArray *languages;
  GHashTable *hash_table;

  GString *name_buf;
  GString *code_buf;
  gboolean in_lingvo;
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
  entry->trie = pdb_trie_builder_new ();
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

  /* This intialises the hash table with the entries found in the list
   * of languages. The key is the language code and the value is the
   * language entry node. The key is owned by the entry node */

  for (i = 0; i < lang->languages->len; i++)
    {
      PdbLangEntry *entry = &g_array_index (lang->languages, PdbLangEntry, i);

      g_hash_table_insert (lang->hash_table, entry->code, entry);
    }
}

PdbLang *
pdb_lang_new (PdbRevo *revo,
              GError **error)
{
  PdbLang *lang = g_slice_new (PdbLang);

  lang->languages = g_array_new (FALSE, FALSE, sizeof (PdbLangEntry));
  lang->parser = pdb_xml_parser_new (revo);

  lang->in_lingvo = FALSE;
  lang->name_buf = g_string_new (NULL);
  lang->code_buf = g_string_new (NULL);
  lang->hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  pdb_xml_set_user_data (lang->parser, lang);

  pdb_xml_set_element_handler (lang->parser,
                               pdb_lang_start_element_cb,
                               pdb_lang_end_element_cb);
  pdb_xml_set_character_data_handler (lang->parser, pdb_lang_character_data_cb);

  if (pdb_xml_parse (lang->parser,
                     "revo/cfg/lingvoj.xml",
                     error))
    {
      pdb_lang_init_hash_table (lang);

      qsort (lang->languages->data,
             lang->languages->len,
             sizeof (PdbLangEntry),
             pdb_lang_compare_name);
    }
  else
    {
      pdb_lang_free (lang);

      lang = NULL;
    }

  return lang;
}

PdbTrieBuilder *
pdb_lang_get_trie (PdbLang *lang,
                   const char *lang_code)
{
  PdbLangEntry *entry = g_hash_table_lookup (lang->hash_table, lang_code);

  if (entry)
    return entry->trie;
  else
    return NULL;
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
      pdb_trie_builder_free (entry->trie);
    }

  g_array_free (lang->languages, TRUE);

  pdb_xml_parser_free (lang->parser);

  g_string_free (lang->name_buf, TRUE);
  g_string_free (lang->code_buf, TRUE);

  g_hash_table_destroy (lang->hash_table);

  g_slice_free (PdbLang, lang);
}
