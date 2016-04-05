#!/usr/bin/env bats

# common functions
load swupdlib

setup() {
  DIR=$(init_web_dir "$srcdir/web-dir")
  export DIR

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core test-bundle

  set_os_release 10 os-core
  set_os_release 10 test-bundle
  track_bundle 10 os-core
  track_bundle 10 test-bundle

  gen_file_plain 10 test-bundle foo
}

@test "full run update creation" {
  sudo $srcdir/swupd_create_update --osversion 10 --statedir $DIR --format 3
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 10
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 test-bundle

  # zero packs should exist (non-zero size) for the build
  [ -s $DIR/www/10/pack-os-core-from-0.tar ]
  [ -s $DIR/www/10/pack-test-bundle-from-0.tar ]

  [[ 1 -eq $(grep '^includes:	os-core$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/usr/share/clear/bundles/test-bundle$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/foo$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/usr/share/clear/bundles/os-core$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 1 -eq $(grep '/usr$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 1 -eq $(grep '/usr/lib$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 1 -eq $(grep '/usr/share$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 1 -eq $(grep '/usr/share/clear$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 1 -eq $(grep '/usr/share/clear/bundles$' $DIR/www/10/Manifest.os-core | wc -l) ]]
  [[ 4 -eq $(tar -tf $DIR/www/10/pack-test-bundle-from-0.tar | wc -l) ]]
  [[ 9 -eq $(tar -tf $DIR/www/10/pack-os-core-from-0.tar | wc -l) ]]
}

teardown() {
  sudo rm -rf $DIR
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
