#!/usr/bin/env bats

@test "make_pack required arg" {
  run $srcdir/swupd_make_pack
  [ "$status" -eq 1 ]
}

@test "make_pack too few arguments" {
  run $srcdir/swupd_make_pack foo
  [ "$status" -eq 1 ]
  run $srcdir/swupd_make_pack foo bar
  [ "$status" -eq 1 ]
}

@test "make_pack too many arguments" {
  run $srcdir/swupd_make_pack foo bar foo bar
  [ "$status" -eq 1 ]
}

@test "make_pack root priv check" {
  run $srcdir/swupd_make_pack foo bar foo
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
