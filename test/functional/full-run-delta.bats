#!/usr/bin/env bats

# common functions
load swupdlib

setup() {
  DIR=$(init_web_dir "$srcdir/web-dir")
  export DIR

  init_server_ini
  init_latest_ver 0
  init_groups_ini os-core test-bundle

  set_os_release 10 os-core
  track_bundle 10 os-core
  track_bundle 10 test-bundle
  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle

  gen_file_to_delta 10 4096 20 4 test-bundle
}

@test "full run update creation with delta packs" {
  # build the first version
  sudo $srcdir/swupd_create_update --osversion 10 --statedir $DIR
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 10
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 10 test-bundle

  # then the second version...
  sudo $srcdir/swupd_create_update --osversion 20 --statedir $DIR
  sudo $srcdir/swupd_make_fullfiles --statedir $DIR 20
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 20 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 0 20 test-bundle

  # and with delta packs this time
  sudo $srcdir/swupd_make_pack --statedir $DIR 10 20 os-core
  sudo $srcdir/swupd_make_pack --statedir $DIR 10 20 test-bundle
}

teardown() {
  sudo rm -rf $DIR
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
