#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  clean_test_dir
  init_test_dir

  init_server_ini
  set_latest_ver 0
  init_groups_ini os-core test-bundle

  set_os_release 10 os-core
  set_os_release 10 test-bundle
  track_bundle 10 os-core
  track_bundle 10 test-bundle

  set_os_release 20 os-core
  set_os_release 20 test-bundle
  track_bundle 20 os-core
  track_bundle 20 test-bundle

  set_os_release 30 os-core
  set_os_release 30 test-bundle
  track_bundle 30 os-core
  track_bundle 30 test-bundle

  gen_file_plain_with_content 10 test-bundle /usr/lib/bar "$(seq 100)"
  gen_file_plain_with_content 20 test-bundle /usr/lib/baz "$(seq 100)"
  # different content just to make sure this works with delta renames as well as
  # direct renames
  gen_file_plain_with_content 30 test-bundle /usr/lib/foo "$(seq 100) new"
}

@test "create updates with renamed-to file getting deleted" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10

  set_latest_ver 10

  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20

  set_latest_ver 20

  sudo $CREATE_UPDATE --osversion 30 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 30
  # version 10: add file to 10
  [ 1 -eq $(grep 'F\.\.\.	.*	10	/usr/lib/bar' $DIR/www/10/Manifest.test-bundle | wc -l) ]
  # version 20: rename bar to baz
  [ 1 -eq $(grep '\.d\.r	.*	20	/usr/lib/bar' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep 'F\.\.r	.*	20	/usr/lib/baz' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  # version 30: prune original renamed-from file (bar), baz is now a renamed-from file
  # Check for the new renamed-to file (foo)
  [ 0 -eq $(grep '/usr/lib/bar' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep '\.d\.r	.*	30	/usr/lib/baz' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep 'F\.\.r	.*	30	/usr/lib/foo' $DIR/www/30/Manifest.test-bundle | wc -l) ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
