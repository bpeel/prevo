#include "config.h"

#include <glib.h>
#include <string.h>

#include "pdb-trie.h"
#include "pdb-strcmp.h"

/* The format of the compressed trie structure consists of a single
 * trie node where a trie node is a recursive variable length
 * structure consisting of :-
 *
 * • A 32 bit number in little endian order. The least significant 31
 *   bits represent the offset in bytes to the next sibling of this
 *   node (including this node).
 * • 1-6 bytes of UTF-8 encoding data to represent the character of
 *   this node.
 * If the most significant bit of the offset is set then the word is a
 * valid entry and will be followed by three bytes:
 *  • Two bytes representing the article number in little endian.
 *  • One byte representing the mark number.
 * Following that there is a list of child nodes of this node.
 */

struct _PdbTrieBuilder
{
  int article_num;
  int mark_num;
  gunichar letter;
  GSList *children;
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
          GSList *prev = NULL, *l;

          child->letter = ch;
          child->article_num = -1;
          child->mark_num = -1;

          /* Find a place to insert this node so the children will
           * remain sorted */
          for (l = builder->children; l; l = l->next)
            {
              PdbTrieBuilder *sibling = l->data;

              if (pdb_strcmp_ch (ch, sibling->letter) <= 0)
                break;

              prev = l;
            }

          if (prev == NULL)
            builder->children = g_slist_prepend (builder->children, child);
          else
            prev->next = g_slist_prepend (prev->next, child);

          builder = child;
        }

      word = g_utf8_next_char (word);
    }

  builder->article_num = article_num;
  builder->mark_num = mark_num;
}

static void
pdb_trie_builder_compress_node (PdbTrieBuilder *builder,
                                GByteArray *data)
{
  int node_start = data->len;
  int character_len;
  guint32 offset;
  GSList *l;

  /* Reserve space for the offset to the next sibling */
  g_byte_array_set_size (data, data->len + sizeof (guint32));

  /* Add a UTF-8 encoded representation of the character of this node */
  g_byte_array_set_size (data, data->len + 6);
  character_len = g_unichar_to_utf8 (builder->letter,
                                     (char *) data->data + data->len - 6);
  g_byte_array_set_size (data, data->len - 6 + character_len);

  /* Optionally add the article and mark number */
  if (builder->article_num >= 0)
    {
      guint16 article_num = GUINT16_TO_LE ((guint16) builder->article_num);
      guint8 mark_num = builder->mark_num;

      g_byte_array_set_size (data, data->len + 3);
      memcpy (data->data + data->len - 3, &article_num, 2);
      memcpy (data->data + data->len - 1, &mark_num, 1);
    }

  /* Add all of the child nodes */
  for (l = builder->children; l; l = l->next)
    pdb_trie_builder_compress_node (l->data, data);

  /* Write the offset */
  offset = data->len - node_start;
  if (builder->article_num >= 0)
    offset |= 1 << (guint32) 31;
  offset = GUINT32_TO_LE (offset);

  memcpy (data->data + node_start, &offset, sizeof (guint32));
}

void
pdb_trie_builder_compress (PdbTrieBuilder *builder,
                           guint8 **data,
                           int *len)
{
  GByteArray *data_buf = g_byte_array_new ();

  pdb_trie_builder_compress_node (builder, data_buf);

  *data = data_buf->data;
  *len = data_buf->len;

  g_byte_array_free (data_buf, FALSE);
}

void
pdb_trie_builder_free (PdbTrieBuilder *builder)
{
  g_slist_foreach (builder->children, (GFunc) pdb_trie_builder_free, NULL);
  g_slist_free (builder->children);
  g_slice_free (PdbTrieBuilder, builder);
}
