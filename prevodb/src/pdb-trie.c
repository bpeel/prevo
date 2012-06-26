#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <ctype.h>
#include <string.h>

#include "pdb-trie.h"

typedef struct _PdbTrieNode PdbTrieNode;

struct _PdbTrieBuilder
{
  int article_num;
  int mark_num;
  gunichar letter;
  GSList *children;
};

static const char pdb_trie_magic[] = "AMDT";
#define PDB_TRIE_MAGIC_LENGTH (G_N_ELEMENTS (pdb_trie_magic) - 1)

#define PDB_TRIE_MAX_NODES ((1 << 25) - 1)

struct _PdbTrieNode
{
  /* If word_level is zero then this combination is not a valid
     word. Otherwise it represents a number of how obscure the word
     is */
  guint32 word_level : 2;
  guint32 letter : 5;
  /* Number of nodes to skip (including this one) to get to the next
     sibling */
  guint32 sibling_offset : 25;
};

struct _PdbTrie
{
  gsize length;
  PdbTrieNode *data;
};

typedef struct _PdbTrieStackEntry PdbTrieStackEntry;

struct _PdbTrieStackEntry
{
  PdbTrieNode *node;
  PdbTrieNode *children_end;
};

PdbTrieBuilder *
pdb_trie_builder_new (void)
{
  PdbTrieBuilder *root = g_slice_new (PdbTrieBuilder);

  root->article_num = -1;
  root->mark_num = -1;
  root->letter = '['; /* This isn't used anywhere. [ is the next
                         character after Z */
  root->children = NULL;

  return root;
}

void
pdb_trie_builder_add_word (PdbTrieBuilder *builder,
                           const char *word,
                           int article_num,
                           int mark_num)
{
  while (*word)
    {
      GSList *l;
      gunichar ch = g_utf8_get_char (word);

      /* Look for a child with this letter */
      for (l = builder->children; l; l = l->next)
        {
          PdbTrieBuilder *child = l->data;

          if (child->letter == ch)
            {
              builder = child;
              break;
            }
        }

      /* If we didn't find a child then add another */
      if (l == NULL)
        {
          PdbTrieBuilder *child = pdb_trie_builder_new ();
          child->letter = ch;
          child->article_num = -1;
          child->mark_num = -1;
          builder->children = g_slist_prepend (builder->children, child);
          builder = child;
        }

      word = g_utf8_next_char (word);
    };

  builder->article_num = article_num;
  builder->mark_num = mark_num;
}

static guint
pdb_trie_builder_get_size (PdbTrieBuilder *builder)
{
  guint size = 1;
  GSList *l;

  for (l = builder->children; l; l = l->next)
    size += pdb_trie_builder_get_size (l->data);

  return size;
}

static guint
pdb_trie_builder_compress_node (PdbTrieBuilder *builder, PdbTrieNode *data)
{
  guint offset = 1;
  GSList *l;

  data->word_level = /* builder->word_level */ -1;
  data->letter = builder->letter - 'A';

  for (l = builder->children; l; l = l->next)
    offset += pdb_trie_builder_compress_node (l->data, data + offset);

  data->sibling_offset = offset;

  return offset;
}

PdbTrie *
pdb_trie_builder_compress (PdbTrieBuilder *builder)
{
  PdbTrie *trie = g_new (PdbTrie, 1);

  trie->length = pdb_trie_builder_get_size (builder);
  trie->data = g_new (PdbTrieNode, trie->length);

  g_assert (trie->length <= PDB_TRIE_MAX_NODES);

  pdb_trie_builder_compress_node (builder, trie->data);

  return trie;
}

void
pdb_trie_builder_free (PdbTrieBuilder *builder)
{
  g_slist_foreach (builder->children, (GFunc) pdb_trie_builder_free, NULL);
  g_slist_free (builder->children);
  g_slice_free (PdbTrieBuilder, builder);
}

gboolean
pdb_trie_lookup_full (const PdbTrie *trie,
                     const gchar *word,
                     guint *word_level)
{
  const PdbTrieNode *node = trie->data;

  while (*word)
    {
      char ch = g_ascii_toupper (*word);
      const PdbTrieNode *children_end = node + node->sibling_offset;
      const PdbTrieNode *child;

      g_return_val_if_fail (ch >= 'A' && ch <= 'Z', FALSE);

      for (child = node + 1;
           child < children_end && child->letter != ch - 'A';
           child += child->sibling_offset);

      if (child >= children_end)
        return FALSE;

      node = child;
      word++;
    }

  /* If the word level is 0 then this isn't a valid place to stop */
  if (node->word_level == 0)
    return FALSE;

  if (word_level)
    *word_level = node->word_level;

  return TRUE;
}

gboolean
pdb_trie_lookup (const PdbTrie *trie, const gchar *word)
{
  return pdb_trie_lookup_full (trie, word, NULL);
}

void
pdb_trie_free (PdbTrie *trie)
{
  g_free (trie->data);
  g_free (trie);
}

void
pdb_trie_foreach (const PdbTrie *trie,
                 PdbTrieCallback callback,
                 gpointer data)
{
  GArray *stack = g_array_new (FALSE, FALSE, sizeof (PdbTrieStackEntry));
  GString *word = g_string_new ("*");
  PdbTrieStackEntry *stack_entry;

  g_array_set_size (stack, 1);
  stack_entry = &g_array_index (stack, PdbTrieStackEntry, 0);
  stack_entry->node = trie->data + 1;
  stack_entry->children_end = trie->data + trie->data->sibling_offset;

  while (stack->len)
    {
      stack_entry = &g_array_index (stack, PdbTrieStackEntry, stack->len - 1);
      if (stack_entry->node < stack_entry->children_end)
        {
          PdbTrieNode *node = stack_entry->node;
          stack_entry->node += node->sibling_offset;
          g_array_set_size (stack, stack->len + 1);
          stack_entry = &g_array_index (stack, PdbTrieStackEntry,
                                        stack->len - 1);
          stack_entry->node = node + 1;
          stack_entry->children_end = node + node->sibling_offset;

          g_string_append_c (word, node->letter + 'A');
          if (node->word_level)
            callback (word->str + 1, node->word_level, data);
        }
      else
        {
          g_array_set_size (stack, stack->len - 1);
          g_string_truncate (word, word->len - 1);
        }
    }

  g_string_free (word, TRUE);
  g_array_free (stack, TRUE);
}

static gboolean
pdb_trie_read (GIOChannel *channel,
              gpointer buf,
              gsize count,
              GError **error)
{
  gsize bytes_read;

  /* This wraps g_io_channel_read_chars and sets a format error if
     there is an unexpected EOF */
  switch (g_io_channel_read_chars (channel, buf, count, &bytes_read, error))
    {
    case G_IO_STATUS_AGAIN:
    case G_IO_STATUS_ERROR:
      /* glib should set an error for us in this case */
      return FALSE;

    case G_IO_STATUS_NORMAL:
      /* If we didn't get the full data then the next call should be EOF */
      if (bytes_read >= count)
        return TRUE;
      /* flow through */
    case G_IO_STATUS_EOF:
      g_set_error (error, PDB_TRIE_ERROR, PDB_TRIE_ERROR_INVALID,
                   "Invalid format");
      return FALSE;
    }

  g_assert_not_reached ();
}

static gboolean
pdb_trie_check_magic (GIOChannel *channel, GError **error)
{
  char magic_check[PDB_TRIE_MAGIC_LENGTH];

  /* Check the file magic header */
  if (pdb_trie_read (channel, magic_check, PDB_TRIE_MAGIC_LENGTH, error))
    {
      if (memcmp (magic_check, pdb_trie_magic, PDB_TRIE_MAGIC_LENGTH))
        {
          g_set_error (error, PDB_TRIE_ERROR, PDB_TRIE_ERROR_NOT_TRIE,
                       "Dictionary file is not in am-trie format");
          return FALSE;
        }
      else
        return TRUE;
    }
  else
    return FALSE;
}

static gboolean
pdb_trie_get_uint32 (GIOChannel *channel, guint32 *val, GError **error)
{
  if (pdb_trie_read (channel, val, sizeof (guint32), error))
    {
      *val = GUINT32_FROM_LE (*val);
      return TRUE;
    }
  else
    return FALSE;
}

PdbTrie *
pdb_trie_load (const gchar *filename, GError **error)
{
  GIOChannel *channel;
  PdbTrie *trie = NULL;

  if ((channel = g_io_channel_new_file (filename, "r", error)))
    {
      guint32 n_nodes;

      /* We want binary data */
      g_io_channel_set_encoding (channel, NULL, NULL);

      if (pdb_trie_check_magic (channel, error)
          && pdb_trie_get_uint32 (channel, &n_nodes, error))
        {
          guint32 i, node_val;
          PdbTrieNode *p;

          trie = g_new (PdbTrie, 1);
          trie->length = n_nodes;
          trie->data = g_new (PdbTrieNode, n_nodes);

          for (i = 0, p = trie->data; i < n_nodes; i++, p++)
            if (pdb_trie_get_uint32 (channel, &node_val, error))
              {
                p->word_level = node_val & 3;
                p->letter = (node_val & 0x7c) >> 2;
                p->sibling_offset = (node_val & 0xffffff80) >> 7;
              }
            else
              {
                pdb_trie_free (trie);
                trie = NULL;
              }

          /* If we managed to read all of the nodes then the file
             could still be invalid if we haven't got to the end */
          if (trie)
            {
              gsize dummy;
              switch (g_io_channel_read_chars (channel, (gchar *) &node_val, 1,
                                               &dummy, error))
                {
                case G_IO_STATUS_NORMAL:
                  g_set_error (error, PDB_TRIE_ERROR, PDB_TRIE_ERROR_INVALID,
                               "Invalid format");
                  /* flow through */
                case G_IO_STATUS_ERROR:
                case G_IO_STATUS_AGAIN:
                  pdb_trie_free (trie);
                  trie = NULL;
                  break;

                case G_IO_STATUS_EOF:
                  break;
                }
            }
        }

      g_io_channel_unref (channel);
    }

  return trie;
}

gboolean
pdb_trie_save (const PdbTrie *trie, const char *filename, GError **error)
{
  GIOChannel *channel;
  gboolean ret = TRUE;

  if ((channel = g_io_channel_new_file (filename, "w", error)))
    {
      gsize bytes_written;
      guint32 n_nodes = GUINT32_TO_LE (trie->length);

      /* We want binary data */
      g_io_channel_set_encoding (channel, NULL, NULL);

      if (g_io_channel_write_chars (channel,
                                    pdb_trie_magic,
                                    PDB_TRIE_MAGIC_LENGTH,
                                    &bytes_written,
                                    error) != G_IO_STATUS_NORMAL
          || g_io_channel_write_chars (channel,
                                       (gchar *) &n_nodes,
                                       sizeof (n_nodes),
                                       &bytes_written,
                                       error) != G_IO_STATUS_NORMAL)
        ret = FALSE;
      else
        {
          guint32 i, node_val;
          const PdbTrieNode *p;

          for (i = 0, p = trie->data; i < n_nodes; i++, p++)
            {
              node_val = GUINT32_TO_LE (p->word_level
                                        | (p->letter << 2)
                                        | (p->sibling_offset << 7));

              if (g_io_channel_write_chars (channel,
                                            (gchar *) &node_val,
                                            sizeof (node_val),
                                            &bytes_written,
                                            error) != G_IO_STATUS_NORMAL)
                {
                  ret = FALSE;
                  break;
                }
            }
        }

      g_io_channel_unref (channel);

      /* If the write failed then delete the file */
      if (!ret)
        g_remove (filename);
    }

  return ret;
}

GQuark
pdb_trie_error_quark (void)
{
  return g_quark_from_static_string ("am-trie-error-quark");
}
