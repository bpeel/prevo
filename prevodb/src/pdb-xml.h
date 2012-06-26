#ifndef PDB_XML_H
#define PDB_XML_H

#include <glib.h>

#include "pdb-revo.h"

/* The PdbXmlParser wraps an XML_Parser from Expat but encapsulates
 * away the fact that the document can reference external entities */
typedef struct _PdbXmlParser PdbXmlParser;

typedef enum
{
  PDB_XML_ERROR_ABORTED = 35
} PdbXmlErrorCode;

typedef void
(* PdbXmlStartElementHandler) (void *user_data,
                               const char *name,
                               const char **atts);

typedef void
(* PdbXmlEndElementHandler) (void *userData,
                             const char *name);


typedef void
(* PdbXmlCharacterDataHandler) (void *user_data,
                                const char *s,
                                int len);

PdbXmlParser *
pdb_xml_parser_new (PdbRevo *revo);

void
pdb_xml_set_element_handler (PdbXmlParser *parser,
                             PdbXmlStartElementHandler start,
                             PdbXmlEndElementHandler end);

void
pdb_xml_set_character_data_handler (PdbXmlParser *parser,
                                    PdbXmlCharacterDataHandler handler);

void
pdb_xml_abort (PdbXmlParser *parser,
               GQuark domain,
               gint code,
               const char *format,
               ...);

void
pdb_xml_abort_error (PdbXmlParser *parser,
                     GError *error);

void
pdb_xml_parser_free (PdbXmlParser *parser);

void
pdb_xml_parser_reset (PdbXmlParser *parser);

void
pdb_xml_set_user_data (PdbXmlParser *parser,
                       void *user_data);

PdbXmlErrorCode
pdb_xml_get_error_code (PdbXmlParser *parser);

int
pdb_xml_get_current_line_number (PdbXmlParser *parser);

int
pdb_xml_get_current_column_number (PdbXmlParser *parser);

const char *
pdb_xml_get_current_filename (PdbXmlParser *parser);

const char *
pdb_xml_error_string (PdbXmlErrorCode code);

gboolean
pdb_xml_get_attribute (const char *element_name,
                       const char **atts,
                       const char *attribute_name,
                       const char **value,
                       GError **error);

gboolean
pdb_xml_parse (PdbXmlParser *parser,
               const char *filename,
               GError **error);

#endif /* PDB_XML_H */
