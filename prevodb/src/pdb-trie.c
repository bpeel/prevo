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
 *
 * If bit 31 of the offset is set then the word is a valid entry and
 * will be followed by a list of article and mark numbers:
 *
 *  • Two bytes representing the article number in little endian.
 *  • One byte representing the mark number.
 *
 * And two most significant bits of the article number have a special
 * meaning:
 *
 *  Bit 15: If this is set then there is a subsequent article after
 *          this one matching this index name. Otherwise this article
 *          represents the end of the list.
 *  Bit 14: If this is set then this entry has a different display
 *          name from its name in the trie. In that case immediately
 *          following these three bytes will be a single byte
 *          representing the length of the display in bytes, followed
 *          by the name itself.
 *
 * Following that there is a list of child nodes of this node.
 */

typedef struct
{
  int article_num;
  int mark_num;
  /* This will be NULL unless the display name for this article is
   * different from the string to search for stored in the trie */
  char *display_word;

  void *data;
} PdbTrieArticle;

typedef struct
{
  GSList *articles;
  GSList *children;
  gunichar letter;
} PdbTrieNode;

struct _PdbTrie
{
  PdbTrieNode *root;
  PdbTrieFreeDataCb free_data_cb;
  PdbTrieGetReferenceCb get_reference_cb;
  void *user_data;
};

static PdbTrieNode *
pdb_trie_node_new (void)
{
  PdbTrieNode *node = g_slice_new (PdbTrieNode);

  node->letter = '['; /* This isn't used anywhere. [ is the next
                         character after Z */
  node->articles = NULL;
  node->children = NULL;

  return node;
}

PdbTrie *
pdb_trie_new (PdbTrieFreeDataCb free_data_cb,
              PdbTrieGetReferenceCb get_reference_cb,
              void *user_data)
{
  PdbTrie *trie = g_slice_new (PdbTrie);
  PdbTrieNode *root = pdb_trie_node_new ();

  trie->free_data_cb = free_data_cb;
  trie->get_reference_cb = get_reference_cb;
  trie->user_data = user_data;
  trie->root = root;

  return trie;
}

void
pdb_trie_add_word (PdbTrie *trie,
                   const char *word,
                   const char *display_word,
                   void *data)
{
  PdbTrieNode *node = trie->root;
  PdbTrieArticle *article;

  while (*word)
    {
      GSList *l;
      gunichar ch = g_utf8_get_char (word);

      /* Look for a child with this letter */
      for (l = node->children; l; l = l->next)
        {
          PdbTrieNode *child = l->data;

          if (child->letter == ch)
            {
              node = child;
              break;
            }
        }

      /* If we didn't find a child then add another */
      if (l == NULL)
        {
          PdbTrieNode *child = pdb_trie_node_new ();
          GSList *prev = NULL, *l;

          child->letter = ch;

          /* Find a place to insert this node so the children will
           * remain sorted */
          for (l = node->children; l; l = l->next)
            {
              PdbTrieNode *sibling = l->data;

              if (pdb_strcmp_ch (ch, sibling->letter) <= 0)
                break;

              prev = l;
            }

          if (prev == NULL)
            node->children = g_slist_prepend (node->children, child);
          else
            prev->next = g_slist_prepend (prev->next, child);

          node = child;
        }

      word = g_utf8_next_char (word);
    }

  article = g_slice_new (PdbTrieArticle);
  article->data = data;
  if (display_word)
    article->display_word = g_strdup (display_word);
  else
    article->display_word = NULL;
  node->articles = g_slist_prepend (node->articles, article);
}

static void
pdb_trie_compress_node (PdbTrie *trie,
                        PdbTrieNode *node,
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
  character_len = g_unichar_to_utf8 (node->letter,
                                     (char *) data->data + data->len - 6);
  g_byte_array_set_size (data, data->len - 6 + character_len);

  /* Optionally add the articles */
  for (l = node->articles; l; l = l->next)
    {
      PdbTrieArticle *article = l->data;
      int article_num_val;
      int mark_num_val;
      guint16 article_num;
      guint8 mark_num = article->mark_num;

      trie->get_reference_cb (article->data,
                              &article_num_val,
                              &mark_num_val,
                              trie->user_data);

      article_num = article_num_val;
      mark_num = mark_num_val;

      if (l->next != NULL)
        article_num |= 0x8000;
      if (article->display_word)
        article_num |= 0x4000;

      article_num = GUINT16_TO_LE (article_num);

      g_byte_array_set_size (data, data->len + 3);
      memcpy (data->data + data->len - 3, &article_num, 2);
      memcpy (data->data + data->len - 1, &mark_num, 1);

      if (article->display_word)
        {
          int len = strlen (article->display_word);
          g_byte_array_set_size (data, data->len + 1 + len);
          data->data[data->len - len - 1] = len;
          memcpy (data->data + data->len - len,
                  article->display_word,
                  len);
        }
    }

  /* Add all of the child nodes */
  for (l = node->children; l; l = l->next)
    pdb_trie_compress_node (trie, l->data, data);

  /* Write the offset */
  offset = data->len - node_start;
  if (node->articles)
    offset |= 1 << (guint32) 31;
  offset = GUINT32_TO_LE (offset);

  memcpy (data->data + node_start, &offset, sizeof (guint32));
}

void
pdb_trie_compress (PdbTrie *trie,
                   guint8 **data,
                   int *len)
{
  GByteArray *data_buf = g_byte_array_new ();

  pdb_trie_compress_node (trie, trie->root, data_buf);

  *data = data_buf->data;
  *len = data_buf->len;

  g_byte_array_free (data_buf, FALSE);
}

static void
pdb_trie_article_free (PdbTrieArticle *article,
                       PdbTrie *trie)
{
  if (trie->free_data_cb)
    trie->free_data_cb (article->data,
                        trie->user_data);

  g_free (article->display_word);
  g_slice_free (PdbTrieArticle, article);
}

static void
pdb_trie_free_node (PdbTrieNode *node,
                    PdbTrie *trie)
{
  g_slist_foreach (node->children, (GFunc) pdb_trie_free_node, trie);
  g_slist_free (node->children);

  g_slist_foreach (node->articles,
                   (GFunc) pdb_trie_article_free,
                   trie);
  g_slist_free (node->articles);

  g_slice_free (PdbTrieNode, node);
}

void
pdb_trie_free (PdbTrie *trie)
{
  pdb_trie_free_node (trie->root, trie);
  g_slice_free (PdbTrie, trie);
}

gboolean
pdb_trie_is_empty (PdbTrie *trie)
{
  return trie->root->children == NULL;
}
