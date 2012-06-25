bin_PROGRAMS = prevodb

INCLUDES = \
	@GLIB_CFLAGS@ \
	$(NULL)

prevodb_SOURCES = \
	main.c \
	pdb-error.c \
	pdb-error.h \
	pdb-lang.c \
	pdb-lang.h \
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
