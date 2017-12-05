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

  set_os_release 40 os-core
  set_os_release 40 test-bundle
  track_bundle 40 os-core
  track_bundle 40 test-bundle

  # /usr/lib/bar and /one will be renamed to /usr/lib/baz and /two
  gen_file_plain_with_content 10 test-bundle /usr/lib/bar "$(seq 100)"
  gen_file_plain_with_content 10 test-bundle /one "$(printf 'a%.0s' {1..200})"
  gen_file_plain_with_content 10 test-bundle /usr/lib/kernel/a "$(printf 'a%.0s' {1..210})"

  gen_file_plain_with_content 20 test-bundle /usr/lib/baz "$(seq 100)"
  gen_file_plain_with_content 20 test-bundle /two "$(printf 'a%.0s' {1..200})"
  gen_file_plain_with_content 20 test-bundle /usr/lib/kernel/ab "$(printf 'a%.0s' {1..210})"

  # different content just to make sure this works with delta renames as well as
  # direct renames
  gen_file_plain_with_content 30 test-bundle /usr/lib/foo "$(seq 100) new"
  gen_file_plain_with_content 30 test-bundle /usr/lib/kernel/abc "$(printf 'a%.0s' {1..210})"

  gen_file_plain_with_content 40 test-bundle /usr/lib/foo "$(seq 100) new"
  gen_file_plain_with_content 40 test-bundle /usr/lib/kernel/abc "$(printf 'a%.0s' {1..210})"
  # new file to force manifest generation
  gen_file_plain_with_content 40 test-bundle /a "testfile"
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

  set_latest_ver 30

  sudo $CREATE_UPDATE --osversion 40 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 40
  # version 10: add files to 10
  [ 1 -eq $(grep $'F\.\.\.\t.*\t10\t/usr/lib/bar' $DIR/www/10/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.\.\.\t.*\t10\t/one' $DIR/www/10/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.b\.\t.*\t10\t/usr/lib/kernel/a' $DIR/www/10/Manifest.test-bundle | wc -l) ]
  # version 20: rename bar to baz and one to two
  [ 1 -eq $(grep $'\.d\.r\t.*\t20\t/one' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.\.r\t.*\t20\t/two' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.r\t.*\t20\t/usr/lib/bar' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.\.r\t.*\t20\t/usr/lib/baz' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.gbr\t.*\t20\t/usr/lib/kernel/a' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.br\t.*\t20\t/usr/lib/kernel/ab' $DIR/www/20/Manifest.test-bundle | wc -l) ]
  # version 30: original renamed-from file (bar) is now orphaned and therefore
  # deleted, baz is now a renamed-from file and foo is a renamed-to file.
  # /two was deleted in this version, so both /one and /two should be marked as
  # deleted
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t20\t/one' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t30\t/two' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t20\t/usr/lib/bar' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.r\t.*\t30\t/usr/lib/baz' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.\.r\t.*\t30\t/usr/lib/foo' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.gbr\t.*\t30\t/usr/lib/kernel/ab' $DIR/www/30/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.br\t.*\t30\t/usr/lib/kernel/abc' $DIR/www/30/Manifest.test-bundle | wc -l) ]

  # version 40: the existing rename from baz -> foo must persist while all
  # others remain deleted
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t20\t/one' $DIR/www/40/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t30\t/two' $DIR/www/40/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.\.\t0\{64\}\t20\t/usr/lib/bar' $DIR/www/40/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'\.d\.r\t.*\t30\t/usr/lib/baz' $DIR/www/40/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.\.r\t.*\t30\t/usr/lib/foo' $DIR/www/40/Manifest.test-bundle | wc -l) ]
  [ 1 -eq $(grep $'F\.b\.\t.*\t30\t/usr/lib/kernel/abc' $DIR/www/40/Manifest.test-bundle | wc -l) ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
