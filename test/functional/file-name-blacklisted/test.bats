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

  gen_file_plain 10 os-core "semicolon;"
}

assert_illegal_char() {
  local output="$1"
  local str=$(echo "Filename $2 includes illegal character(s)")
  [[ "$output" =~ "$str" ]]
}

assert_valid_char() {
  local output="$1"
  local str=$(echo "Filename $2 includes illegal character(s)")
  [[ ! "$output" =~ "$str" ]]
}

@test "file names with blacklisted characters" {
  run sudo sh -c "$CREATE_UPDATE --osversion 10 --statedir $DIR --format 3"

  echo "$output"
  assert_illegal_char "$output" "/semicolon;"
  assert_valid_char "$output" "/usr"
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
