#ifndef PDB_ERROR_H
#define PDB_ERROR_H

#include <glib.h>

#include "pdb-xml.h"

#define PDB_ERROR (pdb_error_quark ())

typedef enum
{
  PDB_ERROR_BAD_FORMAT,
  PDB_ERROR_ABORTED,
  PDB_ERROR_UNZIP_FAILED,
  PDB_ERROR_PARSE
} PdbError;

GQuark
pdb_error_quark (void);

void
pdb_error_from_parser (PdbXmlParser *parser,
                       GError **error);

#endif /* PDB_ERROR_H */
