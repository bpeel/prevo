bin_PROGRAMS = prevodb

INCLUDES = \
	@GLIB_CFLAGS@ \
	$(NULL)

prevodb_SOURCES = \
	main.c \
	pdb-db.c \
	pdb-db.h \
	pdb-doc.c \
	pdb-doc.h \
	pdb-error.c \
	pdb-error.h \
	pdb-lang.c \
	pdb-lang.h \
	pdb-mkdir.c \
	pdb-mkdir.h \
	pdb-revo.c \
	pdb-revo.h \
	pdb-strcmp.c \
	pdb-strcmp.h \
	pdb-trie.c \
	pdb-trie.h \
	pdb-xml.c \
	pdb-xml.h \
	$(NULL)

prevodb_LDADD = \
	-lexpat \
	@GLIB_LIBS@ \
	$(NULL)

noinst_PROGRAMS = dump-index test-doc

dump_index_SOURCES = \
	dump-index.c

dump_index_LDADD = \
	@GLIB_LIBS@ \
	$(NULL)

test_doc_SOURCES = \
	pdb-doc.c \
	pdb-error.c \
	pdb-revo.c \
	pdb-xml.c \
	test-doc.c \
	$(NULL)

test_doc_LDADD = \
	@GLIB_LIBS@ \
	$(NULL)

