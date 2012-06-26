#include "config.h"

#include "pdb-error.h"

GQuark
pdb_error_quark (void)
{
  return g_quark_from_static_string ("pdb-error-quark");
}

void
pdb_error_from_parser (PdbXmlParser *parser,
                       GError **error)
{
  int code;

  switch (pdb_xml_get_error_code (parser))
    {
    case PDB_XML_ERROR_ABORTED:
      code = PDB_ERROR_ABORTED;
      break;

    default:
      code = PDB_ERROR_PARSE;
      break;
    }

  g_set_error (error, PDB_ERROR, code, "%s:%i:%i %s",
               pdb_xml_get_current_filename (parser),
               (int) pdb_xml_get_current_line_number (parser),
               (int) pdb_xml_get_current_column_number (parser),
               pdb_xml_error_string (pdb_xml_get_error_code (parser)));
}
