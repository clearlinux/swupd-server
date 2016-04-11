#!/usr/bin/env bats

load "../swupdlib"

@test "create_update required format" {
  run $CREATE_UPDATE
  [ "$status" -eq 1 ]
  [[ "$output" =~ "Missing format parameter" ]]
}

@test "create_update required version" {
  run $CREATE_UPDATE -F 3
  [ "$status" -eq 1 ]
  [[ "$output" =~ "Missing version parameter:" ]]
}

@test "create_update root priv check" {
  run $CREATE_UPDATE -F 3 -o 10
  [ "$status" -eq 1 ]
  [[ "$output" =~ "not being run as root.. exiting" ]]
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
