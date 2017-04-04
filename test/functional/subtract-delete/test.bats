#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core test-bundle

  # start with both bundles containing a file "foo"
  set_os_release 10 os-core
  track_bundle 10 os-core
  track_bundle 10 test-bundle
  gen_file_plain 10 os-core foo
  gen_file_plain 10 test-bundle foo

  # delete "foo" from os-core (the included bundle)
  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle
  gen_file_plain 20 test-bundle foo

  # delete "foo" from test-bundle
  set_os_release 30 os-core
  track_bundle 30 os-core
  track_bundle 30 test-bundle

  # make modification (add new file) to test-bundle
  set_os_release 40 os-core
  track_bundle 40 os-core
  track_bundle 40 test-bundle
  gen_file_plain 40 test-bundle foobar
}

@test "no subtraction for two deleted files" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  set_latest_ver 10
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  set_latest_ver 20
  sudo $CREATE_UPDATE --osversion 30 --statedir $DIR --format 3
  set_latest_ver 30
  sudo $CREATE_UPDATE --osversion 40 --statedir $DIR --format 3
  set_latest_ver 40

  # If the file is absent in test-bundle, this means it was subtracted. If
  # present, subtraction was not performed.
  hash1=$(hash_for 40 test-bundle "/foo")
  [ -n "$hash1" ]
}


# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
