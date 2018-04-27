bin_PROGRAMS = burro-engine
lib_LTLIBRARIES = libburrocanvas.la

################################################################
# TOP-LEVEL SOURCES

DOT_X_FILES = \
 burro_app_win.x \
 burro_debug_window.x \
 canvas.x \
 canvas_vram.x \
 canvas_bg.x \
 canvas_audio.x

libburrocanvas_la_SOURCES = \
 canvas.c canvas.h canvas.x \
 canvas_audio.c canvas_audio.h canvas_audio.x \
 canvas_pulseaudio.c canvas_pulseaudio.h canvas_pulseaudio.x \
 canvas_bg.c canvas_bg.h canvas_bg.x \
 canvas_colors.c canvas_colors.h \
 canvas_lib.c canvas_lib.h \
 canvas_vram.c canvas_vram.h canvas_vram.x \
 visibility.h 

burro_engine_SOURCES = \
 8x13.h \
 burro_app.c burro_app.h \
 burro_app_win.c burro_app_win.h burro_app_win.x \
 burro_console.c burro_console.h \
 burro_debug_window.c burro_debug_window.h burro_debug_window.x \
 burro_lisp.c burro_lisp.h \
 burro_lineedit.c burro_lineedit.h \
 burro_journal.c burro_journal.h \
 burro_paths.h \
 burro_preferences_dialog.c burro_preferences_dialog.h \
 burro_repl.c burro_repl.h \
 burro_resources.h \
 main.c \
 r/burro.resource.c r/burro.resource.h

BUILT_SOURCES = \
 $(DOT_X_FILES) \
 r/burro.resource.c \
 r/burro.resource.h \
 burro_paths.h

DOT_UI_FILES = \
 r/app-menu.ui \
 r/window.ui \
 r/debug-window.ui \
 r/burro_preferences_dialog.ui \
 r/gears-menu.ui

EXTRA_DIST = r/burro.resource.xml $(DOT_UI_FILES)

r/burro.resource.c: r/burro.resource.xml $(DOT_UI_FILES)
	$(GLIB_COMPILE_RESOURCES) --sourcedir=r --generate-source $<

r/burro.resource.h: r/burro.resource.xml
	$(GLIB_COMPILE_RESOURCES) --sourcedir=r --generate-header $<

burro_paths.h: Makefile
	$(AM_V_GEN)rm -f $@-t && \
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "'`$(CYGPATH_W) $(prefix)`'"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DDDATADIR "'`$(CYGPATH_W) $(datadir)`'"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define RUNSTATEDIR "$(runstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define GUILESITEDIR "$(guilesitedir)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	} | sed '/""/d' > $@-t && \
	mv -f $@-t $@

CLEANFILES = $(DOT_X_FILES) burro_paths.h

burro_engine_CFLAGS = \
  -std=c11 \
  -fvisibility=hidden \
  -DG_LOG_USE_STRUCTURED \
  -DG_LOG_DOMAIN=\"burro\" \
  $(GTK3_CFLAGS) \
  $(GUILE_CFLAGS)
burro_engine_LDADD = $(GTK3_LIBS) $(GUILE_LIBS) -lburrocanvas -lm
burro_engine_DEPENDENCIES = libburrocanvas.la

libburrocanvas_la_CFLAGS = \
  -std=c11 \
  -fvisibility=hidden \
  -DG_LOG_USE_STRUCTURED \
  -DG_LOG_DOMAIN=\"burro-canvas\" \
  $(GTK3_CFLAGS) \
  $(GUILE_CFLAGS) \
  $(VORBISFILE_CFLAGS) \
  $(LIBPULSE_CFLAGS)
libburrocanvas_la_LDFLAGS = -no-undefined -Wl,--no-undefined
libburrocanvas_la_LIBADD = $(GTK3_LIBS) $(GUILE_LIBS) $(VORBISFILE_LIBS) $(LIBPULSE_LIBS) -lm

snarfcppopts = $(DEFS) $(GTK3_CFLAGS) $(GUILE_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(AM_CFLAGS)

SUFFIXES = .x

.c.x:
	$(GUILE_SNARF) -o $@ $< $(snarfcppopts)

gsettings_SCHEMAS = com.lonelycactus.burroengine.gschema.xml
EXTRA_DIST += $(gsettings_SCHEMAS) $(DOT_UI_FILES)

@GSETTINGS_RULES@


################################################################
# Guile scheme libraries

burroguilesitedir=$(guilesitedir)/burro
burroprocessguilesitedir=$(burroguilesitedir)/process

dist_guilesite_DATA = burro.scm

dist_burroguilesite_DATA = \
 burro/colors.scm \
 burro/debug.scm \
 burro/drivers.scm \
 burro/error.scm \
 burro/pm.scm \
 burro/xml.scm

dist_burroprocessguilesite_DATA = \
 burro/process/base.scm \
 burro/process/call-procedure.scm \
 burro/process/fade.scm \
 burro/process/text-click.scm \
 burro/process/wait-on-click.scm \
 burro/process/wait.scm

