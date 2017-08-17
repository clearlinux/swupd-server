#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  init_groups_ini os-core test-bundle1 test-bundle2

  set_os_release 10 os-core
  track_bundle 10 os-core
  track_bundle 10 test-bundle1
  track_bundle 10 test-bundle2
  gen_file_plain 10 test-bundle1 foo
  gen_file_plain 10 test-bundle1 foobar
  gen_file_plain 10 test-bundle2 foo2
  gen_includes_file test-bundle2 10 test-bundle1

  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle1
  track_bundle 20 test-bundle2
  gen_file_plain 20 test-bundle1 foo
  gen_file_plain 20 test-bundle1 foobar
  gen_file_plain 20 test-bundle1 foobarbaz
  gen_file_plain 20 test-bundle2 foo2
  gen_file_plain 20 test-bundle2 foo2bar
  gen_includes_file test-bundle2 20 test-bundle1
}

@test "correct contentsize" {
  # create a couple updates to both check that contentsize does not add included
  # bundles and to verify that files changed in previous updates are counted.
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  set_latest_ver 10

  # contentsize for test-bundle2 should not include test-bundle1's contentsize
  [[ 1 -eq $(grep '^contentsize:	11$' $DIR/www/10/Manifest.test-bundle1 | wc -l) ]]
  [[ 1 -eq $(grep '^contentsize:	5$'  $DIR/www/10/Manifest.test-bundle2 | wc -l) ]]
  # os-core is large because it includes /usr/*
  [[ 1 -eq $(grep '^contentsize:	5134$'  $DIR/www/10/Manifest.os-core | wc -l) ]]
  # 5134 + 11 + 5 = 5150
  [[ 1 -eq $(grep '^contentsize:	5150$' $DIR/www/10/Manifest.full | wc -l) ]]

  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  set_latest_ver 20

  # one new file: foobarbaz (10 bytes)
  [[ 1 -eq $(grep '^contentsize:	21$' $DIR/www/20/Manifest.test-bundle1 | wc -l) ]]
  # one new file: foo2bar (8 bytes)
  [[ 1 -eq $(grep '^contentsize:	13$'  $DIR/www/20/Manifest.test-bundle2 | wc -l) ]]
  # os-core should not change size
  [[ 1 -eq $(grep '^contentsize:	5134$'  $DIR/www/10/Manifest.os-core | wc -l) ]]
  # contentsize for full should be all files, including ones not changed in this release
  # two new files: foo2bar (8 bytes) and foobarbaz (10 bytes)
  # 5150 + 10 + 8  = 5168
  # 5134 + 21 + 13 = 5168
  [[ 1 -eq $(grep '^contentsize:	5168$' $DIR/www/20/Manifest.full | wc -l) ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
