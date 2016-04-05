#!/usr/bin/env bats

# common functions
load swupdlib

setup() {
  DIR=$(init_web_dir "$srcdir/web-dir")
  export DIR

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
  track_bundle 20 included-nested

  gen_file_to_delta 10 4096 20 4 test-bundle

  gen_file_plain 10 test-bundle foo
  gen_file_plain 10 test-bundle foobarbaz
  gen_file_plain 20 test-bundle foo
  gen_file_plain 20 included foo
  gen_file_plain 20 included-two foo
  gen_file_plain 20 included-two foobar
  gen_file_plain 20 included-nested foobarbaz

  gen_includes_file test-bundle 20 included included-two
  gen_includes_file included 20 included-nested
}

@test "full run update creation with delta packs" {
  # build the first version
  sudo $srcdir/swupd_create_update --osversion 10 --statedir $DIR --format 3
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 10
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  # then the second version...
  sudo $srcdir/swupd_create_update --osversion 20 --statedir $DIR --format 3
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 20
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 20 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 20 test-bundle

  # and with delta packs this time
  sudo $srcdir/swupd_make_pack --statedir $DIR 10 20 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 10 20 test-bundle

  # zero packs should exist (non-zero size) for both versions
  [ -s $DIR/www/10/pack-os-core-from-0.tar ]
  [ -s $DIR/www/10/pack-test-bundle-from-0.tar ]
  [ -s $DIR/www/20/pack-os-core-from-0.tar ]
  [ -s $DIR/www/20/pack-test-bundle-from-0.tar ]

  # and delta packs should exist (non-zero size) for the latest version
  [ -s $DIR/www/20/pack-os-core-from-10.tar ]
  [ -s $DIR/www/20/pack-test-bundle-from-10.tar ]

  [[ 1 -eq $(grep '^includes:	os-core$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/foo$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/foobarbaz$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '/foobar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	os-core$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	included$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	included-two$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^.d..	0000000000000000000000000000000000000000000000000000000000000000	20	/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^.d..	0000000000000000000000000000000000000000000000000000000000000000	20	/foobarbaz$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/foo$' $DIR/www/20/Manifest.included | wc -l) ]]
  [[ 1 -eq $(grep '/foo$' $DIR/www/20/Manifest.included-two | wc -l) ]]
  [[ 1 -eq $(grep '/foobar$' $DIR/www/20/Manifest.included-two | wc -l) ]]
  [[ 1 -eq $(grep '/foobarbaz$' $DIR/www/20/Manifest.included-nested | wc -l) ]]
  [[ 6 -eq $(tar -tf $DIR/www/10/pack-test-bundle-from-0.tar | wc -l) ]]
  [[ 4 -eq $(tar -tf $DIR/www/20/pack-test-bundle-from-0.tar | wc -l) ]]
}

teardown() {
  sudo rm -rf $DIR
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
