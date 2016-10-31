#!/usr/bin/env bats

load "../swupdlib"

@test "make_fullfiles required arg" {
  run $MAKE_FULLFILES
  [ "$status" -eq 1 ]
}

@test "make_fullfiles too many arguments" {
  # exactly one argument must be passed
  run $MAKE_FULLFILES foo bar
  [ "$status" -eq 1 ]
}

@test "make_fullfiles root priv check" {
  [ $EUID -eq 0 ] && skip "test can only be run as non-root"
  run $MAKE_FULLFILES foo
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
