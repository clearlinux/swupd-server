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
  track_bundle 10 os-core
  track_bundle 10 test-bundle
  set_os_release 20 os-core
  track_bundle 20 os-core
  track_bundle 20 test-bundle

  # Files have different names ("foo" vs "bar"), but have the same content
  gen_file_plain_with_content 10 test-bundle "foo" "data"
  gen_file_plain_with_content 20 test-bundle "bar" "data"
}

@test "rename detection support" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core
  sudo $MAKE_PACK --statedir $DIR 10 20 test-bundle

  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "rename detection support 1 to 2" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  gen_file_plain_with_content 20 test-bundle "baz" "data"
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core
  sudo $MAKE_PACK --statedir $DIR 10 20 test-bundle

  # A renamed file comprises a new file and a deleted file.
  # Should be one file that is not renamed
  # Need to figure out what happens to hard links
  [[ 1 -eq $(grep -c '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle) ]]
  [[ 1 -eq $(grep -c '^F\.\.\..*/ba[rz]$' $DIR/www/20/Manifest.test-bundle) ]]
  [[ 1 -eq $(grep -c '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle) ]]
}

@test "rename detection support 2 to 2" {
  gen_file_plain_with_content 10 test-bundle "fee" "data"
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  gen_file_plain_with_content 20 test-bundle "foo~" "data"
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core
  sudo $MAKE_PACK --statedir $DIR 10 20 test-bundle

  # A renamed file comprises a new file and a deleted file.
  # Should be one file that is not renamed
  # Need to figure out what happens to hard links
  [[ 1 -eq $(grep -c '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle) ]]
  [[ 1 -eq $(grep -c '^F\.\.r.*/foo~$' $DIR/www/20/Manifest.test-bundle) ]]
  [[ 2 -eq $(grep -c '^\.d\.r.*/f..$' $DIR/www/20/Manifest.test-bundle) ]]
}

@test "rename detection support binary files" {
  sudo mkdir -p  $DIR/image/10/test-bundle/bin/
  sudo cp /bin/gcc-ar $DIR/image/10/test-bundle/bin/gcc-ar.1
  sudo cp /bin/gcc-ar $DIR/image/20/test-bundle/gcc-ar.1
  strip $DIR/image/20/test-bundle/gcc-ar.1
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  gen_file_plain_with_content 20 test-bundle "foo~" "And now not the same"
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core
  sudo $MAKE_PACK --statedir $DIR 10 20 test-bundle

  # A renamed file comprises a new file and a deleted file.
  # Should be one file that is not renamed
  # Need to figure out what happens to hard links
}

@test "rename detection support binary files 2" {
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core
  sudo $MAKE_PACK --statedir $DIR 0 10 test-bundle

  set_latest_ver 10

  gen_file_plain_with_content 20 test-bundle "foo" "And now not the same"
  gen_file_plain_with_content 20 test-bundle "foo.1" "data"
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core
  sudo $MAKE_PACK --statedir $DIR 0 20 test-bundle
  sudo $MAKE_PACK --statedir $DIR 10 20 os-core
  sudo $MAKE_PACK --statedir $DIR 10 20 test-bundle

  # A renamed file comprises a new file and a deleted file.
  # Should be one file that is not renamed
  # Need to figure out what happens to hard links
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
