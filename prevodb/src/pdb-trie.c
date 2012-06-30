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
} PdbTrieBuilderArticle;

struct _PdbTrieBuilder
{
  GSList *articles;
  GSList *children;
  gunichar letter;
};

PdbTrieBuilder *
pdb_trie_builder_new (void)
{
  PdbTrieBuilder *root = g_slice_new (PdbTrieBuilder);

  root->letter = '['; /* This isn't used anywhere. [ is the next
                         character after Z */
  root->articles = NULL;
  root->children = NULL;

  return root;
}

void
pdb_trie_builder_add_word (PdbTrieBuilder *builder,
                           const char *word,
                           const char *display_word,
                           int article_num,
                           int mark_num)
{
  PdbTrieBuilderArticle *article;

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

  article = g_slice_new (PdbTrieBuilderArticle);
  article->article_num = article_num;
  article->mark_num = mark_num;
  if (display_word)
    article->display_word = g_strdup (display_word);
  else
    article->display_word = NULL;
  builder->articles = g_slist_prepend (builder->articles, article);
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

  /* Optionally add the articles */
  for (l = builder->articles; l; l = l->next)
    {
      PdbTrieBuilderArticle *article = l->data;
      guint16 article_num = article->article_num;
      guint8 mark_num = article->mark_num;

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
  for (l = builder->children; l; l = l->next)
    pdb_trie_builder_compress_node (l->data, data);

  /* Write the offset */
  offset = data->len - node_start;
  if (builder->articles)
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

static void
pdb_trie_builder_article_free (PdbTrieBuilderArticle *article)
{
  g_free (article->display_word);
  g_slice_free (PdbTrieBuilderArticle, article);
}

void
pdb_trie_builder_free (PdbTrieBuilder *builder)
{
  g_slist_foreach (builder->children, (GFunc) pdb_trie_builder_free, NULL);
  g_slist_free (builder->children);

  g_slist_foreach (builder->articles,
                   (GFunc) pdb_trie_builder_article_free,
                   NULL);
  g_slist_free (builder->articles);

  g_slice_free (PdbTrieBuilder, builder);
}
