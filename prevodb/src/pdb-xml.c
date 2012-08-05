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

#include <expat.h>
#include <string.h>
#include <stdarg.h>

#include "pdb-xml.h"
#include "pdb-error.h"

typedef struct
{
  XML_Parser parser;
  const char *filename;
} PdbXmlStackEntry;

struct _PdbXmlParser
{
  PdbRevo *revo;

  GArray *stack;
  void *user_data;

  GError *abort_error;

  PdbXmlStartElementHandler start_element_handler;
  PdbXmlEndElementHandler end_element_handler;
  PdbXmlCharacterDataHandler character_data_handler;
};

static void
pdb_xml_init_parser (PdbXmlParser *parser,
                     XML_Parser xml_parser);

gboolean
pdb_xml_get_attribute (const XML_Char *element_name,
                       const XML_Char **atts,
                       const char *attribute_name,
                       const char **value,
                       GError **error)
{
  while (atts[0])
    {
      if (!strcmp (atts[0], attribute_name))
        {
          *value = atts[1];
          return TRUE;
        }

      atts += 2;
    }

  g_set_error (error, PDB_ERROR, PDB_ERROR_BAD_FORMAT,
               "Missing attribute “%s” on element “%s”",
               attribute_name,
               element_name);

  return FALSE;
}

static XML_Parser
pdb_xml_get_parser (PdbXmlParser *parser)
{
  return g_array_index (parser->stack,
                        PdbXmlStackEntry,
                        parser->stack->len - 1).parser;
}

static gboolean
pdb_xml_handle_data (PdbXmlParser *parser,
                     const char *buf,
                     int len,
                     gboolean end,
                     GError **error)
{
  XML_Parser xml_parser = pdb_xml_get_parser (parser);

  if (XML_Parse (xml_parser, buf, len, end) == XML_STATUS_ERROR)
    {
      switch (XML_GetErrorCode (xml_parser))
        {
        case XML_ERROR_ABORTED:
        case XML_ERROR_EXTERNAL_ENTITY_HANDLING:
          g_propagate_error (error, parser->abort_error);
          parser->abort_error = NULL;
          return FALSE;

        default:
          pdb_error_from_parser (parser, error);
          return FALSE;
        }
    }
  else
    return TRUE;
}

static void
pdb_xml_start_element_cb (void *user_data,
                          const XML_Char *name,
                          const XML_Char **atts)
{
  PdbXmlParser *parser = user_data;

  if (parser->start_element_handler)
    parser->start_element_handler (parser->user_data, name, atts);
}

static void
pdb_xml_end_element_cb (void *user_data,
                        const XML_Char *name)
{
  PdbXmlParser *parser = user_data;

  if (parser->end_element_handler)
    parser->end_element_handler (parser->user_data, name);
}

static void
pdb_xml_character_data_cb (void *user_data,
                           const XML_Char *s,
                           int len)
{
  PdbXmlParser *parser = user_data;

  if (parser->character_data_handler)
    parser->character_data_handler (parser->user_data, s, len);
}

gboolean
pdb_xml_parse (PdbXmlParser *parser,
               const char *filename,
               GError **error)
{
  PdbRevoFile *file;
  gboolean ret = TRUE;
  const char *last_slash;

  if ((last_slash = strrchr (filename, '/')))
    {
      char *base = g_strndup (filename, last_slash - filename);
      XML_SetBase (pdb_xml_get_parser (parser), base);
      g_free (base);
    }

  g_array_index (parser->stack,
                 PdbXmlStackEntry,
                 parser->stack->len - 1).filename = filename;

  file = pdb_revo_open (parser->revo,
                        filename,
                        error);

  if (file == NULL)
    ret = FALSE;
  else
    {
      char buf[512];
      size_t got;

      do
        {
          got = sizeof (buf);
          if (!pdb_revo_read (file, buf, &got, error) ||
              !pdb_xml_handle_data (parser,
                                    buf,
                                    got,
                                    got < sizeof (buf),
                                    error))
            {
              ret = FALSE;
              break;
            }
        }
      while (got >= sizeof (buf));

      pdb_revo_close (file);
    }

  g_array_index (parser->stack,
                 PdbXmlStackEntry,
                 parser->stack->len - 1).filename = NULL;

  return ret;
}

static int
pdb_xml_external_entity_ref_cb (XML_Parser xml_parser,
                                const XML_Char *context,
                                const XML_Char *base,
                                const XML_Char *system_id,
                                const XML_Char *public_id)
{
  PdbXmlParser *parser = XML_GetUserData (xml_parser);
  PdbXmlStackEntry *stack_top;
  XML_Parser external_parser;
  GError *error = NULL;
  char *filename;
  gboolean ret;

  if (system_id == NULL)
    {
      g_set_error (&parser->abort_error,
                   PDB_ERROR,
                   PDB_ERROR_BAD_FORMAT,
                   "%s:%i:%i: An external entity was encountered "
                   "without a system id",
                   pdb_xml_get_current_filename (parser),
                   pdb_xml_get_current_line_number (parser),
                   pdb_xml_get_current_column_number (parser));
      return XML_STATUS_ERROR;
    }

  external_parser = XML_ExternalEntityParserCreate (xml_parser,
                                                    context,
                                                    NULL);
  pdb_xml_init_parser (parser, external_parser);

  if (base)
    filename = g_strconcat (base, "/", system_id, NULL);
  else
    filename = g_strdup (system_id);

  g_array_set_size (parser->stack, parser->stack->len + 1);
  stack_top = &g_array_index (parser->stack,
                              PdbXmlStackEntry,
                              parser->stack->len - 1);
  stack_top->parser = external_parser;
  stack_top->filename = NULL;

  ret = pdb_xml_parse (parser, filename, &error);

  g_free (filename);

  XML_ParserFree (external_parser);

  g_array_set_size (parser->stack, parser->stack->len - 1);

  if (ret)
    return XML_STATUS_OK;
  else
    {
      parser->abort_error = error;
      return XML_STATUS_ERROR;
    }
}

static void
pdb_xml_init_parser (PdbXmlParser *parser,
                     XML_Parser xml_parser)
{
  XML_SetParamEntityParsing (xml_parser,
                             XML_PARAM_ENTITY_PARSING_ALWAYS);

  XML_SetUserData (xml_parser, parser);

  XML_SetElementHandler (xml_parser,
                         pdb_xml_start_element_cb,
                         pdb_xml_end_element_cb);
  XML_SetCharacterDataHandler (xml_parser,
                               pdb_xml_character_data_cb);

  XML_SetExternalEntityRefHandler (xml_parser,
                                   pdb_xml_external_entity_ref_cb);
}

PdbXmlParser *
pdb_xml_parser_new (PdbRevo *revo)
{
  PdbXmlParser *parser = g_slice_new (PdbXmlParser);
  PdbXmlStackEntry *entry;

  parser->stack = g_array_new (FALSE, FALSE, sizeof (PdbXmlStackEntry));
  parser->user_data = NULL;
  parser->abort_error = NULL;
  parser->revo = revo;
  parser->start_element_handler = NULL;
  parser->end_element_handler = NULL;
  parser->character_data_handler = NULL;

  g_array_set_size (parser->stack, 1);
  entry = &g_array_index (parser->stack, PdbXmlStackEntry, 0);

  entry->parser = XML_ParserCreate (NULL);
  pdb_xml_init_parser (parser, entry->parser);

  return parser;
}

void
pdb_xml_set_element_handler (PdbXmlParser *parser,
                             PdbXmlStartElementHandler start,
                             PdbXmlEndElementHandler end)
{
  parser->start_element_handler = start;
  parser->end_element_handler = end;
}

void
pdb_xml_set_character_data_handler (PdbXmlParser *parser,
                                    PdbXmlCharacterDataHandler handler)
{
  parser->character_data_handler = handler;
}

void
pdb_xml_abort_error (PdbXmlParser *parser,
                     GError *error)
{
  g_assert (parser->abort_error == NULL);

  parser->abort_error = error;

  XML_StopParser (pdb_xml_get_parser (parser), FALSE);
}

void
pdb_xml_abort (PdbXmlParser *parser,
               GQuark domain,
               gint code,
               const char *format,
               ...)
{
  va_list ap;
  GError *error = NULL;
  GString *message = g_string_new (NULL);

  g_assert (parser->abort_error == NULL);

  g_string_append_printf (message,
                          "%s:%i:%i: ",
                          pdb_xml_get_current_filename (parser),
                          pdb_xml_get_current_line_number (parser),
                          pdb_xml_get_current_column_number (parser));

  va_start (ap, format);
  g_string_append_vprintf (message, format, ap);
  va_end (ap);

  g_set_error_literal (&error, domain, code, message->str);

  g_string_free (message, TRUE);

  pdb_xml_abort_error (parser, error);
}

void
pdb_xml_parser_free (PdbXmlParser *parser)
{
  int i;

  for (i = parser->stack->len - 1; i >= 0; i--)
    {
      PdbXmlStackEntry *entry =
        &g_array_index (parser->stack, PdbXmlStackEntry, i);
      XML_ParserFree (entry->parser);
    }

  g_array_free (parser->stack, TRUE);
  g_slice_free (PdbXmlParser, parser);
}

void
pdb_xml_parser_reset (PdbXmlParser *parser)
{
  XML_Parser xml_parser = pdb_xml_get_parser (parser);

  g_assert (parser->stack->len == 1);

  XML_ParserReset (xml_parser, NULL);
  pdb_xml_init_parser (parser, xml_parser);
}

void
pdb_xml_set_user_data (PdbXmlParser *parser,
                       void *user_data)
{
  parser->user_data = user_data;
}

PdbXmlErrorCode
pdb_xml_get_error_code (PdbXmlParser *parser)
{
  return XML_GetErrorCode (pdb_xml_get_parser (parser));
}

int
pdb_xml_get_current_line_number (PdbXmlParser *parser)
{
  return XML_GetCurrentLineNumber (pdb_xml_get_parser (parser));
}

int
pdb_xml_get_current_column_number (PdbXmlParser *parser)
{
  return XML_GetCurrentColumnNumber (pdb_xml_get_parser (parser));
}

const char *
pdb_xml_get_current_filename (PdbXmlParser *parser)
{
  return g_array_index (parser->stack,
                        PdbXmlStackEntry,
                        parser->stack->len - 1).filename;
}

const char *
pdb_xml_error_string (PdbXmlErrorCode code)
{
  return XML_ErrorString ((enum XML_Error) code);
}
