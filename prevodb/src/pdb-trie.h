#ifndef PDB_TRIE_H
#define PDB_TRIE_H

#include <glib.h>

#define PDB_TRIE_ERROR pdb_trie_error_quark ()

#define PDB_TRIE_MAX_WORD_LEVEL 3

typedef struct _PdbTrieBuilder PdbTrieBuilder;
typedef struct _PdbTrie PdbTrie;

typedef enum
{
  PDB_TRIE_ERROR_INVALID,
  PDB_TRIE_ERROR_NOT_TRIE
} PdbTrieError;

typedef void
(* PdbTrieCallback) (const gchar *word,
                     guint word_level,
                     gpointer data);

void
pdb_trie_foreach (const PdbTrie *trie,
                  PdbTrieCallback callback,
                  gpointer data);

PdbTrie *
pdb_trie_load (const gchar *filename, GError **error);

gboolean
pdb_trie_save (const PdbTrie *trie,
               const char *filename,
               GError **error);

PdbTrieBuilder *
pdb_trie_builder_new (void);

void
pdb_trie_builder_add_word (PdbTrieBuilder *builder,
                           const gchar *word,
                           int article_num,
                           int mark_num);

PdbTrie *
pdb_trie_builder_compress (PdbTrieBuilder *builder);

void
pdb_trie_builder_free (PdbTrieBuilder *builder);

gboolean
pdb_trie_lookup_full (const PdbTrie *trie, const gchar *word,
                      guint *word_level);

gboolean
pdb_trie_lookup (const PdbTrie *trie, const gchar *word);

void
pdb_trie_free (PdbTrie *trie);

GQuark
pdb_trie_error_quark (void);

#endif /* PDB_TRIE_H */
