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

  set_os_release 30 os-core
  track_bundle 30 os-core
}

@test "full run update creation with delta packs over format bump" {
  # build the first version
  echo $CREATE_UPDATE
  echo $DIR
  sudo $CREATE_UPDATE --osversion 10 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 10
  sudo $MAKE_PACK --statedir $DIR 0 10 os-core

  set_latest_ver 10

  # then the second version...
  echo $CREATE_UPDATE
  echo $DIR
  sudo $CREATE_UPDATE --osversion 20 --statedir $DIR --format 3
  sudo $MAKE_FULLFILES --statedir $DIR 20
  sudo $MAKE_PACK --statedir $DIR 0 20 os-core

  set_latest_ver 20

  # then the third version...
  echo $CREATE_UPDATE
  echo $DIR
  sudo $CREATE_UPDATE --osversion 30 --statedir $DIR --format 4
  sudo $MAKE_FULLFILES --statedir $DIR 30
  sudo $MAKE_PACK --statedir $DIR 0 30 os-core

  # zero packs should exist (non-zero size) for all versions
  [ -s $DIR/www/10/pack-os-core-from-0.tar ]
  [ -s $DIR/www/20/pack-os-core-from-0.tar ]
  [ -s $DIR/www/30/pack-os-core-from-0.tar ]

  [[ 0 -eq $(grep '^actions:	update$' $DIR/www/10/Manifest.MoM | wc -l) ]]
  [[ 0 -eq $(grep '^actions:	update$' $DIR/www/20/Manifest.MoM | wc -l) ]]
  [[ 1 -eq $(grep '^actions:	update$' $DIR/www/30/Manifest.MoM | wc -l) ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
