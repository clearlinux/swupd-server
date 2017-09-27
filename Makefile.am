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
	src/create_update.c \
	src/delta.c \
	src/fullfiles.c \
	src/globals.c \
	src/groups.c \
	src/helpers.c \
	src/heuristics.c \
	src/log.c \
	src/in_memory_archive.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/stats.c \
	src/type_change.c \
	src/versions.c \
	src/xattrs.c

swupd_make_pack_SOURCES = \
	src/analyze_fs.c \
	src/config.c \
	src/delta.c \
	src/globals.c \
	src/groups.c \
	src/helpers.c \
	src/log.c \
	src/make_packs.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/stats.c \
	src/xattrs.c

swupd_make_fullfiles_SOURCES = \
	src/analyze_fs.c \
	src/config.c \
	src/delta.c \
	src/fullfiles.c \
	src/globals.c \
	src/groups.c \
	src/helpers.c \
	src/in_memory_archive.c \
	src/log.c \
	src/make_fullfiles.c \
	src/manifest.c \
	src/pack.c \
	src/rename.c \
	src/stats.c \
	src/xattrs.c

AM_CPPFLAGS = $(glib_CFLAGS) $(libarchive_CFLAGS) -I$(top_srcdir)/include

swupd_create_update_LDADD = \
	$(glib_LIBS) \
	$(zlib_LIBS) \
	$(openssl_LIBS) \
	$(libarchive_LIBS) \
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
	$(libarchive_LIBS) \
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

TEST_EXTENSIONS = .bats

if ENABLE_TESTS
tap_driver = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/tap-driver.sh

LOG_DRIVER = $(tap_driver)
BATS_LOG_DRIVER = $(tap_driver)

TESTS = $(dist_check_SCRIPTS)

dist_check_SCRIPTS = \
	test/functional/basic/test.bats \
	test/functional/contentsize-across-versions-includes/test.bats \
	test/functional/delete-no-version-bump/test.bats \
	test/functional/file-name-blacklisted/test.bats \
	test/functional/file-name-debuginfo/test.bats \
	test/functional/format-no-decrement/test.bats \
	test/functional/full-run-delta/test.bats \
	test/functional/full-run/test.bats \
	test/functional/fullfiles/test.bats \
	test/functional/include-version-bump/test.bats \
	test/functional/includes-deduplicate/test.bats \
	test/functional/no-delta/test.bats \
	test/functional/pack/test.bats \
	test/functional/state-file/test.bats \
	test/functional/subtract-delete/test.bats \
	test/functional/update/test.bats \
	test/functional/format-bump/test.bats

if RENAMES
dist_check_SCRIPTS += \
	test/functional/renames/test.bats
endif
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

compliant:
	@git diff --quiet --exit-code include src; ret=$$?; \
	if [ $$ret -eq 1 ]; then \
		echo "Error: can only check code style when include/ and src/ are clean."; \
		echo "Stash or commit your changes and try again."; \
		exit $$ret; \
	elif [ $$ret -gt 1 ]; then \
		exit $$ret; \
	fi; \
	clang-format -i -style=file include/*.h src/*.c; ret=$$?; \
	if [ $$ret -ne 0 ]; then \
		exit $$ret; \
	fi; \
	git diff --quiet --exit-code include src; ret=$$?; \
	if [ $$ret -eq 1 ]; then \
		echo "Code style issues found. Run 'git diff' to view issues."; \
	elif [ $$ret -eq 0 ]; then \
		echo "No code style issues found."; \
	fi; \
	exit $$ret

release:
	@git rev-parse v$(PACKAGE_VERSION) &> /dev/null; \
	if [ "$$?" -eq 0 ]; then \
		echo "Error: Release for $(PACKAGE_VERSION) already exists."; \
		echo "Bump version in configure.ac before releasing."; \
		exit 1; \
	fi
	@git tag -a -m "$(PACKAGE_NAME) release $(PACKAGE_VERSION)" v$(PACKAGE_VERSION)
	@printf "\nNew release $(PACKAGE_VERSION) tagged!\n\n"
