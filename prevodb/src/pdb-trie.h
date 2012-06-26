#ifndef PDB_TRIE_H
#define PDB_TRIE_H

#include <glib.h>

typedef struct _PdbTrieBuilder PdbTrieBuilder;

PdbTrieBuilder *
pdb_trie_builder_new (void);

void
pdb_trie_builder_add_word (PdbTrieBuilder *builder,
                           const gchar *word,
                           int article_num,
                           int mark_num);

void
pdb_trie_builder_compress (PdbTrieBuilder *builder,
                           guint8 **data,
                           int *len);

void
pdb_trie_builder_free (PdbTrieBuilder *builder);

#endif /* PDB_TRIE_H */
