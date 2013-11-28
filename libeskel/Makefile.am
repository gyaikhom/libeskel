#*********************************************************************
#
#  Edinburgh Skeleton Library (eskel)
#
#  THE ENHANCE PROJECT
#  School of Informatics,
#  University of Edinburgh,
#  Edinburgh - EH9 3JZ
#  United Kingdom
#
#  Written by: Gagarine Yaikhom
#
#*********************************************************************

MAINTAINERCLEANFILES = Makefile.in

INCLUDES = -I$(top_builddir) -I$(top_srcdir)

include_HEADERS = eskel.h

pkginclude_HEADERS = \
	htree.h \
	mem.h \
	pepa.h \
	rmon.h \
	sched.h \
	state.h \
	take.h \
	give.h \
	tags.h \
	summ.h \
	coollex.h \
	math.h

lib_LIBRARIES = libeskel.a

libeskel_a_SOURCES = \
	eskel.c \
	htree.c \
	mem.c \
	pepa.c \
	rmon.c \
	sched.c \
	state.c \
	take.c \
	give.c \
	rtime.c \
	summ.c \
	coollex.c \
	math.c

bin_PROGRAMS = eskel_extract

eskel_extract_SOURCES = eskel_extract.c

dist_data_DATA = PEPAWorkbench.jar

# End of file.