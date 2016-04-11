#!/usr/bin/env bats

load "../swupdlib"

@test "create_update help output" {
  run $CREATE_UPDATE --help
  [ "$status" -eq 1 ]
  run $CREATE_UPDATE -h
  [ "$status" -eq 1 ]
}

@test "make_fullfiles help output" {
  run $MAKE_FULLFILES --help
  [ "$status" -eq 1 ]
  run $MAKE_FULLFILES -h
  [ "$status" -eq 1 ]
}

@test "make_pack help output" {
  run $MAKE_PACK --help
  [ "$status" -eq 1 ]
  run $MAKE_PACK -h
  [ "$status" -eq 1 ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
