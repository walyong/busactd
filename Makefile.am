ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = .

CLEANFILES =
EXTRA_DIST =

noinst_LTLIBRARIES =
noinst_DATA =

DEFAULT_CFLAGS = \
	$(OUR_CFLAGS)

DEFAULT_LDFLAGS = \
	$(OUR_LDFLAGS)

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I $(top_srcdir)/src \
	-I $(top_srcdir)/src/shared \
	$(DEFAULT_CFLAGS)

AM_CFLAGS = $(DEFAULT_CFLAGS)
AM_LDFLAGS = $(DEFAULT_LDFLAGS)
AM_LIBS = $(DEFAULT_LIBS)

INSTALL_EXEC_HOOKS =
UNINSTALL_EXEC_HOOKS =

# busactd
busactddir=$(prefix)/lib/busactd
busactd_PROGRAMS=
busactdsystemconfdir=$(busactddir)/system
busactdsystemconf_DATA =
busactduserconfdir=$(busactddir)/user
busactduserconf_DATA =

busactdtestdir=$(busactddir)/test
busactdtest_PROGRAMS=
busactdtest_SCRIPTS=

# DBus
dbussystemservicedir=$(prefix)/share/dbus-1/system-services
dbusservicedir=$(prefix)/share/dbus-1/services
dbussystemservice_DATA =
dbusservice_DATA =

# systemd
systemdsystemunitdir=$(prefix)/lib/systemd/system
systemduserunitdir=$(prefix)/lib/systemd/user
systemdsystemunit_DATA =
systemduserunit_DATA =

SYSTEMD_SYSTEM_MULTI_USER_TARGET_WANTS =
SYSTEMD_USER_DEFAULT_TARGET_WANTS =

systemd-install-target-wants-hook:
	where=$(systemdsystemunitdir) && what="$(SYSTEMD_SYSTEM_MULTI_USER_TARGET_WANTS)" && wants=multi-user.target && $(add-wants)
	where=$(systemduserunitdir) && what="$(SYSTEMD_USER_DEFAULT_TARGET_WANTS)" && wants=default.target && $(add-wants)

define add-wants
[ -z "$$what" -o -z "$$where" ] || ( \
	dir=$(DESTDIR)$$where/$$wants.wants && \
	$(MKDIR_P) -m 0755 $$dir && \
	cd $$dir && \
	rm -f $$what && \
	for i in $$what; do $(LN_S) ../$$i . || exit $$? ; done )
endef

INSTALL_EXEC_HOOKS += \
	systemd-install-target-wants-hook

# ------------------------------------------------------------------------------
noinst_LTLIBRARIES += \
	libbusact-shared.la

libbusact_shared_la_SOURCES = \
	src/shared/log.c

# ------------------------------------------------------------------------------
# busactd
busactd_SOURCES = \
	src/busactd/dbus.c \
	src/busactd/busactd.c \
	src/busactd/main.c

busactd_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBSYSTEM_CFLAGS)

busactd_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(LIBSYSTEM_LIBS) \
	libbusact-shared.la

busactd_PROGRAMS += \
	busactd

systemdsystemunit_DATA += \
	src/busactd/system/busactd.service

dbussystemservice_DATA += \
	src/busactd/system/org.tizen.busactd.service

SYSTEMD_SYSTEM_MULTI_USER_TARGET_WANTS += \
	busactd.service

systemduserunit_DATA += \
	src/busactd/user/busactd.service

dbusservice_DATA += \
	src/busactd/user/org.tizen.busactd.service

SYSTEMD_USER_DEFAULT_TARGET_WANTS += \
	busactd.service

# ------------------------------------------------------------------------------
# busactd test process
test_busactd_SOURCES = \
	src/test/test-busactd.c

test_busactd_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(LIBSYSTEM_CFLAGS)

test_busactd_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(LIBSYSTEM_LIBS) \
	libbusact-shared.la

busactdtest_PROGRAMS += \
	test-busactd

busactdtest_SCRIPTS += \
	src/test/test-busactd-dbus-send.sh

systemdsystemunit_DATA += \
	src/test/system/test-busactd.service

dbussystemservice_DATA += \
	src/test/system/org.tizen.busactd.test.service

busactdsystemconf_DATA += \
	src/test/system/org.tizen.busactd.test.conf

systemduserunit_DATA += \
	src/test/user/test-busactd.service

dbusservice_DATA += \
	src/test/user/org.tizen.busactd.test.service

busactduserconf_DATA += \
	src/test/user/org.tizen.busactd.test.conf

install-exec-hook: $(INSTALL_EXEC_HOOKS)
