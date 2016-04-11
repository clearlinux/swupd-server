#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core test-bundle included included-two included-nested

  set_os_release 10 os-core
  track_bundle 10 os-core
  track_bundle 10 test-bundle

  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle
  track_bundle 20 included
  track_bundle 20 included-two

  set_os_release 30 os-core
  track_bundle 30 os-core
  track_bundle 30 test-bundle
  track_bundle 30 included
  track_bundle 30 included-two
  track_bundle 30 included-nested

  gen_file_plain 10 test-bundle foo
  gen_file_plain 20 test-bundle foo
  gen_file_plain 20 included bar
  gen_file_plain 20 included-two baz
  gen_file_plain 30 test-bundle foo
  gen_file_plain 30 included bar
  gen_file_plain 30 included-two baz
  gen_file_plain 30 included-nested foobarbaz

  gen_includes_file test-bundle 20 included included-two
  gen_includes_file test-bundle 30 included included-two
  gen_includes_file included 30 included-nested
}

@test "full run update creation with delta packs" {
  # build the first version
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  # then the second version...
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 0 20 included
  sudo $MAKE_PACK --statedir $DIR 0 20 included-two

  set_latest_ver 20

  # then the third version...
  sudo $CREATE_UPDATE --osversion 30 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 30
  sudo $MAKE_PACK --statedir $DIR 0 30 os-core
  sudo $MAKE_PACK --statedir $DIR 0 30 test-bundle
  sudo $MAKE_PACK --statedir $DIR 0 30 included
  sudo $MAKE_PACK --statedir $DIR 0 30 included-two
  sudo $MAKE_PACK --statedir $DIR 0 30 included-nested

  # zero packs should exist (non-zero size) for both versions
  [ -s $DIR/www/10/pack-os-core-from-0.tar ]
  [ -s $DIR/www/10/pack-test-bundle-from-0.tar ]
  [ -s $DIR/www/20/pack-os-core-from-0.tar ]
  [ -s $DIR/www/20/pack-test-bundle-from-0.tar ]
  [ -s $DIR/www/20/pack-included-from-0.tar ]
  [ -s $DIR/www/20/pack-included-two-from-0.tar ]
  [ -s $DIR/www/30/pack-os-core-from-0.tar ]
  [ -s $DIR/www/30/pack-test-bundle-from-0.tar ]
  [ -s $DIR/www/30/pack-included-from-0.tar ]
  [ -s $DIR/www/30/pack-included-two-from-0.tar ]
  [ -s $DIR/www/30/pack-included-nested-from-0.tar ]

  [[ 1 -eq $(grep '^includes:	os-core$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/foo$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	os-core$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	included$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	included-two$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/bar$' $DIR/www/20/Manifest.included | wc -l) ]]
  [[ 1 -eq $(grep '/baz$' $DIR/www/20/Manifest.included-two | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	included-nested$' $DIR/www/30/Manifest.included | wc -l) ]]
  [[ 1 -eq $(grep '/foobarbaz$' $DIR/www/30/Manifest.included-nested | wc -l) ]]
  [[ 0 -eq $(ls $DIR/www/30/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(ls $DIR/www/30/Manifest.included-two | wc -l) ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
