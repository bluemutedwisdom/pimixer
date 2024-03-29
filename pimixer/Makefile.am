# $Id$
# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

bin_PROGRAMS =								\
	pimixer

pimixer_SOURCES =							\
	main.c								\
	xfce-mixer-track.c						\
	xfce-mixer-track.h						\
	xfce-mixer-switch.c						\
	xfce-mixer-switch.h						\
	xfce-mixer-option.c						\
	xfce-mixer-option.h						\
	xfce-mixer.c							\
	xfce-mixer.h							\
	xfce-mixer-controls-dialog.c					\
	xfce-mixer-controls-dialog.h					\
	xfce-mixer-window.h						\
	xfce-mixer-window.c

pimixer_CFLAGS = 							\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DDATADIR=\"$(datadir)\"					\
	-DMIXER_DATADIR=\"$(pkgdatadir)\"				\
	$(GLIB_CFLAGS)							\
	$(GTK_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(XFCONF_CFLAGS)						\
	$(GST_PLUGINS_BASE_CFLAGS)

pimixer_DEPENDENCIES = 						\
	$(top_builddir)/libxfce4mixer/libxfce4mixer.la
	
pimixer_LDFLAGS = 							\
	$(top_builddir)/libxfce4mixer/libxfce4mixer.la			\
	$(GLIB_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(GTK_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)							\
	$(GST_PLUGINS_BASE_LIBS)					\
	-lgstaudio-0.10							\
	-lgstinterfaces-0.10

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = pimixer.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 								\
 	$(desktop_in_in_files)
 
DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

# Get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/applications^" 		\
		$< > $@
