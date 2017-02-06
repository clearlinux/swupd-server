#!/usr/bin/env bats

# common functions
load "../swupdlib"

setup() {
  maybeskip
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

  gendataA 10 foo
  gendataA 20 bar
}

# Generate data files of 3 types in test-bundle
# All files are big enough that they get rename detection
gendataA() {
  gen_file_plain_with_content "$1" test-bundle "$2" "$(seq 100)"
}
gendataB() {
  gen_file_plain_with_content "$1" test-bundle "$2" "$(seq 50) $(seq 52 101)"
}
gendataC() {
  # cache string
  [ -z "$dataC" ] || dataC="$(seq 1000 | gzip | uuencode wombat)"
  gen_file_plain_with_content "$1" test-bundle "$2" "$dataC"
}
# Generate small data files
gendataAs() {
  gen_file_plain_with_content "$1" test-bundle "$2" "$(seq 50)"
}
gendataBs() {
  gen_file_plain_with_content "$1" test-bundle "$2" "$(seq 24) $(seq 26 49)"
}
gendataCs() {
  # cache string
  [ -z "$dataCs" ] || dataC="$(seq 50 | gzip | uuencode wombat)"
  gen_file_plain_with_content "$1" test-bundle "$2" "$dataCs"
}

# Guts of doing an update
do_an_update() {
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
}


@test "basic rename detection support" {
  gendataA 10 foo
  gendataA 20 bar
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "ignore rename detection for small files" {
  gendataAs 10 foo
  gendataAs 20 bar
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 0 -eq $(grep '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "ignore rename detection for large to small files" {
  gendataA 10 foo
  gendataAs 20 bar
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 0 -eq $(grep '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "ignore rename detection for small to large files" {
  gendataAs 10 foo
  gendataA 20 bar
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 0 -eq $(grep '^F\.\.r.*/bar$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "rename one file to two" {
  gendataA 10 foo
  gendataA 20 bar
  gendataA 20 baz
  do_an_update  

  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "rename two file to one" {
  gendataA 10 foo
  gendataA 10 foz
  gendataA 20 baz
  do_an_update  

  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/baz$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.\..*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "rename two files to two" {
  gendataA 10 foo
  gendataA 10 foz
  gendataA 20 bar
  gendataA 20 baz  
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 2 -eq $(grep '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 2 -eq $(grep '^\.d\.r.*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "rename two files to two, one slightly different" {
  gendataA 10 foo
  gendataA 10 foz
  gendataA 20 bar
  gendataB 20 baz  
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 2 -eq $(grep '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 2 -eq $(grep '^\.d\.r.*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "rename two files to two, one very different" {
  gendataA 10 foo
  gendataA 10 foz
  gendataA 20 bar
  gendataC 20 baz  
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}
@test "rename two files to two, one small" {
  gendataA 10 foo
  gendataA 10 foz
  gendataA 20 bar
  gendataCs 20 baz  
  do_an_update
  # A renamed file comprises a new file and a deleted file
  [[ 1 -eq $(grep '^F\.\.r.*/ba[rz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^\.d\.r.*/fo[oz]$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "directory name changes" {
  gendataA 10 dir1/foo
  gendataA 20 dir2/foo
  do_an_update
  [[ 1 -eq $(grep '^\.d\.r.*dir1/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 1 -eq $(grep '^F\.\.r.*dir2/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

@test "directory name changes small files" {
  gendataAs 10 dir1/foo
  gendataAs 20 dir2/foo
  do_an_update
  [[ 0 -eq $(grep '^\.d\.r.*dir1/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
  [[ 0 -eq $(grep '^F\.\.r.*dir2/foo$' $DIR/www/20/Manifest.test-bundle | wc -l) ]]
}

# Emacs and vi support
# Local variables:
# sh-indentation: 2
# End:
# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
