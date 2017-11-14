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

  gen_file_plain_with_content 10 test-bundle usr/lib/kernel/bar testfile_contents
  gen_file_plain_with_content 20 test-bundle usr/lib/kernel/baz new_testfile_contents
  # make sure /usr/lib/kernel stays around
  mkdir $DIR/image/30/test-bundle/usr/lib/kernel
}

@test "create updates while ghosting boot files before removing them" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10

  set_latest_ver 10

  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20

  set_latest_ver 20

  sudo $CREATE_UPDATE --osversion 30 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 30
  # version 10: add boot file to 10
  [ 1 -eq $(grep 'F\.b\.	.*	10	/usr/lib/kernel/bar' $DIR/www/10/Manifest.full | wc -l) ]
  # version 20: ghost boot file instead of deleting it when it doesn't exist
  [ 1 -eq $(grep '\.gb\.	.*	20	/usr/lib/kernel/bar' $DIR/www/20/Manifest.full | wc -l) ]
  # version 20: add a new boot file
  [ 1 -eq $(grep 'F\.b\.	.*	20	/usr/lib/kernel/baz' $DIR/www/20/Manifest.full | wc -l) ]
  # version 30: old ghosted file /usr/lib/kernel/bar cleaned up
  [ 0 -eq $(grep '10	/usr/lib/kernel/bar' $DIR/www/30/Manifest.full | wc -l) ]
  # version 30: boot file added in version 20 ghosted
  [ 1 -eq $(grep '\.gb\.	.*	30	/usr/lib/kernel/baz' $DIR/www/30/Manifest.full | wc -l) ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
