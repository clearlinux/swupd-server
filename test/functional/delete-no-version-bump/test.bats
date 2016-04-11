#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

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
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10

  set_latest_ver 10

  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20

  [ 1 -eq $(grep '10	/foo' $DIR/www/10/Manifest.full | wc -l) ]
  [ 1 -eq $(grep '10	/foo' $DIR/www/20/Manifest.full | wc -l) ]
  [ 0 -eq $(grep '20	/foo' $DIR/www/20/Manifest.full | wc -l) ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
