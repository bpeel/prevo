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
