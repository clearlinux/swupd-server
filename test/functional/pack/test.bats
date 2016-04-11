#!/usr/bin/env bats

load "../swupdlib"

@test "make_pack required arg" {
  run $MAKE_PACK
  [ "$status" -eq 1 ]
}

@test "make_pack too few arguments" {
  run $MAKE_PACK foo
  [ "$status" -eq 1 ]
  run $MAKE_PACK foo bar
  [ "$status" -eq 1 ]
}

@test "make_pack too many arguments" {
  run $MAKE_PACK foo bar foo bar
  [ "$status" -eq 1 ]
}

@test "make_pack root priv check" {
  run $MAKE_PACK foo bar foo
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
