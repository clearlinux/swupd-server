#!/usr/bin/env bats

@test "create_update required arg" {
  run $srcdir/swupd_create_update
  [ "$status" -eq 1 ]
  [[ "$output" =~ "Missing version parameter:" ]]
}

@test "create_update root priv check" {
  run $srcdir/swupd_create_update -o 10
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
