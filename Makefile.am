EXTRA_DIST = COPYING

AM_CFLAGS = -fPIC -Iinclude/ -O2 -g -Wall -W -Wformat-security -D_FORTIFY_SOURCE=2 -DSWUPD_LINUX_ROOTFS -fno-common -std=gnu99

bin_PROGRAMS = \
	swupd_create_update \
	swupd_make_pack \
	swupd_make_fullfiles

# TODO:
# check_PROGRAMS =
# ...add test/* binaries as appopropriate

swupd_create_update_SOURCES = \
	src/analyze_fs.c \
	src/chroot.c \
	src/config.c \
	src/delta.c \
	src/fullfiles.c \
	src/globals.c \
	src/groups.c \
	src/helpers.c \
	src/heuristics.c \
	src/log.c \
	src/main.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/signature.c \
	src/stats.c \
	src/type_change.c \
	src/versions.c \
	src/xattrs.c

swupd_make_pack_SOURCES = \
	src/analyze_fs.c \
	src/config.c \
	src/delta.c \
	src/globals.c \
	src/helpers.c \
	src/log.c \
	src/make_packs.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/signature.c \
	src/stats.c \
	src/xattrs.c

swupd_make_fullfiles_SOURCES = \
	src/analyze_fs.c \
	src/config.c \
	src/delta.c \
	src/fullfiles.c \
	src/globals.c \
	src/helpers.c \
	src/log.c \
	src/make_fullfiles.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/signature.c \
	src/stats.c \
	src/xattrs.c

AM_CPPFLAGS = $(glib_CFLAGS) -I$(top_srcdir)/include

swupd_create_update_LDADD = \
	$(glib_LIBS) \
	$(zlib_LIBS) \
	$(openssl_LIBS) \
	$(bsdiff_LIBS)

swupd_make_pack_LDADD = \
	$(glib_LIBS) \
	$(zlib_LIBS) \
	$(openssl_LIBS) \
	$(bsdiff_LIBS)

swupd_make_fullfiles_LDADD = \
	$(glib_LIBS) \
	$(zlib_LIBS) \
	$(openssl_LIBS) \
	$(bsdiff_LIBS)

if ENABLE_LZMA
swupd_create_update_LDADD += \
	$(lzma_LIBS)
swupd_make_pack_LDADD += \
	$(lzma_LIBS)
swupd_make_fullfiles_LDADD += \
	$(lzma_LIBS)
endif

noinst_HEADERS = \
	include/swupd.h \
	include/xattrs.h

TEST_EXTENSIONS = .sh

if ENABLE_TESTS
tap_driver = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh

LOG_DRIVER = $(tap_driver)
SH_LOG_DRIVER = $(tap_driver)

TESTS = $(dist_check_SCRIPTS)

dist_check_SCRIPTS = \
	test/functional/basic.bats \
	test/functional/delete-no-version-bump.bats \
	test/functional/include-version-bump.bats \
	test/functional/update.bats \
	test/functional/fullfiles.bats \
	test/functional/pack.bats \
	test/functional/full-run.bats \
	test/functional/full-run-delta.bats
endif

if COVERAGE
AM_CFLAGS += --coverage

coverage: coverage-clean
	mkdir -p coverage
	lcov --compat-libtool --directory . --capture --output-file coverage/report
	genhtml -o coverage/ coverage/report

coverage-clean:
	rm -rf coverage
endif

distclean-local:
	rm -rf aclocal.m4 autogen.sh autom4te.cache config.h.in config.h.in~ configure depcomp install-sh Makefile.in missing compile

release:
	@git rev-parse v$(PACKAGE_VERSION) &> /dev/null; \
	if [ "$$?" -eq 0 ]; then \
		echo "Error: Release for $(PACKAGE_VERSION) already exists."; \
		echo "Bump version in configure.ac before releasing."; \
		exit 1; \
	fi
	@git tag -a -m "$(PACKAGE_NAME) release $(PACKAGE_VERSION)" v$(PACKAGE_VERSION)
	@printf "\nNew release $(PACKAGE_VERSION) tagged!\n\n"
