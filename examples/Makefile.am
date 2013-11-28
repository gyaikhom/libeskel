################################################################
# Edinburgh Skeleton Library (eskel)
#
# Written by: Gagarine Yaikhom.
# Copyright (2005) University of Edinburgh.
#
# $Id$
# NOTE: Process this file with automake to produce Makefile.in
################################################################

MAINTAINERCLEANFILES = Makefile.in

CC = mpicc

SUBDIRS = 

INCLUDES = -I$(top_builddir) -I$(top_srcdir)

noinst_PROGRAMS = \
	pipeline1 \
	deal1 \
	pmva

pipeline1_SOURCES = pipeline1.c
pipeline1_LDADD = $(top_builddir)/libeskel/libeskel.a

deal1_SOURCES = deal1.c
deal1_LDADD = $(top_builddir)/libeskel/libeskel.a

pmva_SOURCES = pmva.c
pmva_LDADD = $(top_builddir)/libeskel/libeskel.a

# End of file.
