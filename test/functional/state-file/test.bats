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

  gen_file_plain 10 os-core "/var/lib/test"
}

@test "state file marked in manifest" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3

  # a file and dir installed in /var should be marked state
  grep '^D\.s\..*/var/lib$' $DIR/www/10/Manifest.os-core
  grep '^F\.s\..*/var/lib/test$' $DIR/www/10/Manifest.os-core
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
