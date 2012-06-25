#include "config.h"

#include "pdb-error.h"

GQuark
pdb_error_quark (void)
{
  return g_quark_from_static_string ("pdb-error-quark");
}

void
pdb_error_from_parser (XML_Parser parser,
                       const char *filename,
                       GError **error)
{
  int code;

  switch (XML_GetErrorCode (parser))
    {
    case XML_ERROR_ABORTED:
      code = PDB_ERROR_ABORTED;
      break;

    default:
      code = PDB_ERROR_PARSE;
      break;
    }

  g_set_error (error, PDB_ERROR, code, "%s:%i:%i %s",
               filename,
               (int) XML_GetCurrentLineNumber (parser),
               (int) XML_GetCurrentColumnNumber (parser),
               XML_ErrorString (XML_GetErrorCode (parser)));
}
