#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core

  set_os_release 10 os-core
  track_bundle 10 os-core
  set_os_release 20 os-core
  track_bundle 20 os-core

  gen_file_plain 10 os-core foo
  gen_file_plain 10 os-core bar
  gen_file_plain 20 os-core foo
  gen_file_plain 20 os-core baz
}

@test "ensure format numbers cannot be decremented" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  set_latest_ver 10

  # Now, decrement the format number. Should result in an EXIT_FAILURE.
  run sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 2
  echo "$output"
  [ $status -eq 1 ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
