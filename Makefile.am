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

AUX_DIST = \
	$(ac_aux_dir)/config.sub \
	$(ac_aux_dir)/config.guess \
	$(ac_aux_dir)/install-sh \
	$(ac_aux_dir)/missing \
	$(ac_aux_dir)/mkinstalldirs

EXTRA_DIST = VERSION bootstrap m4/get_libeskel_version m4/libeskel_get_version.m4

AUTOMAKE_OPTIONS = gnu

SUBDIRS = libeskel examples

MAINTAINERCLEANFILES = Makefile Makefile.in aclocal.m4 configure $(AUX_DIST)

ACLOCAL = -I ./config

# End of file.
