# NOTE: source this file from a *.bats file

init_web_dir() {
  # absolute path is a hard requirement right now
  local dir=$(realpath $1)
  mkdir -p $dir/{image,www}
  echo $dir
}

init_server_ini() {
  cp $srcdir/server.ini $DIR
  sed -i "s|/var/lib/update|$DIR|" $DIR/server.ini
}

init_latest_ver() {
  echo "$1" > $DIR/image/latest.version
}

init_groups_ini() {
  for bundle in "$@"; do
    cat >> $DIR/groups.ini << EOF
[$bundle]
group=$bundle
EOF
  done
}

set_os_release() {
  local ver=$1
  local bundle=$2
  mkdir -p $DIR/image/$ver/$bundle/usr/lib/
  echo "VERSION_ID=$ver" > $DIR/image/$ver/$bundle/usr/lib/os-release
}

track_bundle() {
  local ver=$1
  local bundle=$2
  mkdir -p $DIR/image/$ver/$bundle/usr/share/clear/bundles
  touch $DIR/image/$ver/$bundle/usr/share/clear/bundles/$bundle
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
