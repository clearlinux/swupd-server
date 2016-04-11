#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

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
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

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

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
