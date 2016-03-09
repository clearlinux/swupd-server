#!/usr/bin/env bats

@test "create_update help output" {
  run $srcdir/swupd_create_update --help
  [ "$status" -eq 1 ]
  run $srcdir/swupd_create_update -h
  [ "$status" -eq 1 ]
}

@test "make_fullfiles help output" {
  run $srcdir/swupd_make_fullfiles --help
  [ "$status" -eq 1 ]
  run $srcdir/swupd_make_fullfiles -h
  [ "$status" -eq 1 ]
}

@test "make_pack help output" {
  run $srcdir/swupd_make_pack --help
  [ "$status" -eq 1 ]
  run $srcdir/swupd_make_pack -h
  [ "$status" -eq 1 ]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
