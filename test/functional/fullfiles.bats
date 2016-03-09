#!/usr/bin/env bats

@test "make_fullfiles required arg" {
  run $srcdir/swupd_make_fullfiles
  [ "$status" -eq 1 ]
}

@test "make_fullfiles too many arguments" {
  # exactly one argument must be passed
  run $srcdir/swupd_make_fullfiles foo bar
  [ "$status" -eq 1 ]
}

@test "make_fullfiles root priv check" {
  run $srcdir/swupd_make_fullfiles foo
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
