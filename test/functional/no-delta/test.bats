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

  # symlink -> regular file type change (L -> F)
  gen_file_to_delta 10 4096 20 4 os-core testfile1
  gen_symlink_to_file 10 os-core testsym1 testfile1
  copy_file 10 os-core testfile1 20 os-core testsym1

  # regular file -> symlink type change (F -> L)
  gen_file_to_delta 10 4096 20 4 os-core testfile2
  copy_file 10 os-core testfile2 10 os-core testsym2
  gen_symlink_to_file 20 os-core testsym2 testfile2

  # symlink change + symlink target change; delta should be created for
  # testfile3, but not for the dereferenced testsym3
  gen_file_to_delta 10 4096 20 4 os-core testfile3
  copy_file 20 os-core testfile3 20 os-core testfile4
  gen_symlink_to_file 10 os-core testsym3 testfile3
  gen_symlink_to_file 20 os-core testsym3 testfile4
}

@test "no deltas created for type changes or dereferenced symlinks" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  set_latest_ver 10
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3

  # attempt to create some deltas
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core

  # F -> F deltas should exist
  hash1=$(hash_for 10 os-core "/testfile1")
  hash2=$(hash_for 20 os-core "/testfile1")
  [ -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]

  hash1=$(hash_for 10 os-core "/testfile2")
  hash2=$(hash_for 20 os-core "/testfile2")
  [ -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]

  hash1=$(hash_for 10 os-core "/testfile3")
  hash2=$(hash_for 20 os-core "/testfile3")
  [ -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]

  # deltas for symlink type changes should not be created
  hash1=$(hash_for 10 os-core "/testsym1")
  hash2=$(hash_for 20 os-core "/testsym1")
  [ ! -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]

  hash1=$(hash_for 10 os-core "/testsym2")
  hash2=$(hash_for 20 os-core "/testsym2")
  [ ! -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]

  hash1=$(hash_for 10 os-core "/testsym3")
  hash2=$(hash_for 20 os-core "/testsym3")
  [ ! -f $DIR/www/20/delta/10-20-$hash1-$hash2 ]
}


# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
