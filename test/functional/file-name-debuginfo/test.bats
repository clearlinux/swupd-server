#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core
  init_groups_ini test-bundle

  set_os_release 10 os-core
  track_bundle 10 os-core
  set_os_release 10 test-bundle
  track_bundle 10 test-bundle

  gen_file_plain 10 test-bundle "/usr/lib/debug/foo"
  gen_file_plain 10 test-bundle "/usr/src/debug/bar"
  gen_file_plain 10 test-bundle "/usr/bin/foobar"
}

@test "debuginfo files pruned" {
  run sudo sh -c "$CREATE_UPDATE --osversion 10 --statedir $DIR --format 3"
  # This should not be pruned
  [[ 1 -eq $(grep '/usr/bin/foobar$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/usr/lib/debug$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '/usr/src/debug$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  # These should be pruned
  [[ 0 -eq $(grep '/usr/src/debug/bar$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '/usr/lib/debug/foo$' $DIR/www/10/Manifest.test-bundle | wc -l) ]]

  echo "$output"
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
