#include "config.h"

#include <expat.h>
#include <string.h>

#include "pdb-xml.h"
#include "pdb-error.h"

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
  return (XML_Parser) parser;
}

typedef struct
{
  const char *filename;
  PdbXmlParser *parser;
} PdbXmlParseData;

static PdbRevoReadStatus
pdb_xml_parse_cb (const char *buf,
                  int len,
                  gboolean end,
                  void *user_data,
                  GError **error)
{
  PdbXmlParseData *data = user_data;
  XML_Parser parser = pdb_xml_get_parser (data->parser);

  if (XML_Parse (parser, buf, len, end) == XML_STATUS_ERROR)
    {
      pdb_error_from_parser (data->parser, data->filename, error);
      if (XML_GetErrorCode (parser) == XML_ERROR_ABORTED)
        return PDB_REVO_READ_STATUS_ABORT;
      else
        return PDB_REVO_READ_STATUS_ERROR;
    }
  else
    return PDB_REVO_READ_STATUS_OK;
}

gboolean
pdb_xml_parse (PdbXmlParser *parser,
               PdbRevo *revo,
               const char *filename,
               GError **error)
{
  PdbXmlParseData data;

  data.filename = filename;
  data.parser = parser;

  return pdb_revo_parse_file (revo,
                              filename,
                              pdb_xml_parse_cb,
                              &data,
                              error);
}

PdbXmlParser *
pdb_xml_parser_new (void)
{
  return (PdbXmlParser *) XML_ParserCreate (NULL);
}

void
pdb_xml_set_element_handler (PdbXmlParser *parser,
                             PdbXmlStartElementHandler start,
                             PdbXmlEndElementHandler end)
{
  XML_SetElementHandler ((XML_Parser) parser, start, end);
}

void
pdb_xml_set_character_data_handler (PdbXmlParser *parser,
                                    PdbXmlCharacterDataHandler handler)
{
  XML_SetCharacterDataHandler ((XML_Parser) parser, handler);
}

void
pdb_xml_stop_parser (PdbXmlParser *parser)
{
  XML_StopParser ((XML_Parser) parser, FALSE);
}

void
pdb_xml_parser_free (PdbXmlParser *parser)
{
  XML_ParserFree ((XML_Parser) parser);
}

void
pdb_xml_parser_reset (PdbXmlParser *parser)
{
  XML_ParserReset ((XML_Parser) parser, NULL);
}

void
pdb_xml_set_user_data (PdbXmlParser *parser,
                       void *user_data)
{
  XML_SetUserData ((XML_Parser) parser, user_data);
}

PdbXmlErrorCode
pdb_xml_get_error_code (PdbXmlParser *parser)
{
  return XML_GetErrorCode ((XML_Parser) parser);
}

int
pdb_xml_get_current_line_number (PdbXmlParser *parser)
{
  return XML_GetCurrentLineNumber ((XML_Parser) parser);
}

int
pdb_xml_get_current_column_number (PdbXmlParser *parser)
{
  return XML_GetCurrentColumnNumber ((XML_Parser) parser);
}

const char *
pdb_xml_error_string (PdbXmlErrorCode code)
{
  return XML_ErrorString ((enum XML_Error) code);
}
