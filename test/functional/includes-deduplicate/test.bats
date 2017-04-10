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
  track_bundle 10 os-core
  track_bundle 10 test-bundle1
  track_bundle 10 test-bundle2

  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle1
  track_bundle 20 test-bundle2

  gen_file_plain 10 os-core test0
  gen_file_plain 10 test-bundle1 test1
  gen_file_plain 10 test-bundle2 test2

  gen_includes_file test-bundle2 10 test-bundle1 test-bundle1
  gen_includes_file test-bundle2 20 test-bundle1 test-bundle1
}

@test "deduplicate bundle includes" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  set_latest_ver 10
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3

  # includes list should be deduplicated in both the old and new manifests
  [[ 1 -eq $(grep '^includes:	test-bundle1$' $DIR/www/10/Manifest.test-bundle2 | wc -l) ]]
  [[ 1 -eq $(grep '^includes:	test-bundle1$' $DIR/www/20/Manifest.test-bundle2 | wc -l) ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
