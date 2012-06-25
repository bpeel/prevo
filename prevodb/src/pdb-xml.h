#ifndef PDB_XML_H
#define PDB_XML_H

#include <expat.h>
#include <glib.h>

gboolean
pdb_xml_get_attribute (const XML_Char *element_name,
                       const XML_Char **atts,
                       const char *attribute_name,
                       const char **value,
                       GError **error);


#endif /* PDB_XML_H */
