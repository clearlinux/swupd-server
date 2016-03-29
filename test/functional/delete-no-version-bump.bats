#!/usr/bin/env bats

# common functions
load swupdlib

setup() {
  DIR=$(init_web_dir "$srcdir/web-dir")
  export DIR

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core test-bundle1 test-bundle2

  set_os_release 10 os-core
  set_os_release 10 test-bundle1
  set_os_release 10 test-bundle2
  track_bundle 10 os-core
  track_bundle 10 test-bundle1
  track_bundle 10 test-bundle2

  set_os_release 20 os-core
  set_os_release 20 test-bundle1
  set_os_release 20 test-bundle2
  track_bundle 20 os-core
  track_bundle 20 test-bundle1
  track_bundle 20 test-bundle2

  gen_file_plain 10 test-bundle1 foo
  gen_file_plain 10 test-bundle2 foo

  gen_file_plain 20 test-bundle1 foo
}

@test "delete no version bump update creation" {
  sudo $srcdir/swupd_create_update --osversion 10 --statedir $DIR
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 10

  set_latest_ver 10

  sudo $srcdir/swupd_create_update --osversion 20 --statedir $DIR
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 20

  [ 1 -eq $(grep '10	/foo' $DIR/www/10/Manifest.full | wc -l) ]
  [ 1 -eq $(grep '10	/foo' $DIR/www/20/Manifest.full | wc -l) ]
  [ 0 -eq $(grep '20	/foo' $DIR/www/20/Manifest.full | wc -l) ]
}

teardown() {
  sudo rm -rf $DIR
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
