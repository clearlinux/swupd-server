# NOTE: source this file from a *.bats file

# The location of the swupd_* binaries
export SRCDIR="$BATS_TEST_DIRNAME/../../../"

export CREATE_UPDATE="$SRCDIR/swupd_create_update"
export MAKE_FULLFILES="$SRCDIR/swupd_make_fullfiles"
export MAKE_PACK="$SRCDIR/swupd_make_pack"

export DIR="$BATS_TEST_DIRNAME/web-dir"

init_test_dir() {
  local testdir="$BATS_TEST_DIRNAME"
  mkdir -p "$testdir"/logs
  mkdir -p $DIR/{image,www}
  # run swupd_* inside the directory to dump the logs
  cd "$testdir"/logs
}

clean_test_dir() {
  sudo rm -rf $DIR "$BATS_TEST_DIRNAME"/logs
}

init_server_ini() {
  cp $SRCDIR/server.ini $DIR
  sed -i "s|/var/lib/update|$DIR|" $DIR/server.ini
}

set_latest_ver() {
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

gen_includes_file() {
  local bundle=$1
  local ver=$2
  local includes="${@:3}"
  mkdir -p $DIR/www/$ver/noship
  for b in $includes; do
    cat >> $DIR/www/$ver/noship/"$bundle"-includes << EOF
$b
EOF
  done
}

gen_file_to_delta() {
  local origver=$1
  local origsize=$2
  local newver=$3
  local newbytes=$4
  local bundle=$5

  # create some random data for the original version
  mkdir -p $DIR/image/$origver/$bundle
  dd if=/dev/urandom of=$DIR/image/$origver/$bundle/randomfile bs=1 count=$origsize

  # append more random data to the end of the file in the new version
  TMP=$(mktemp foo.XXXXXX)
  mkdir -p $DIR/image/$newver/$bundle
  dd if=/dev/urandom of=$TMP bs=1 count=$newbytes
  cat $DIR/image/$origver/$bundle/randomfile $TMP > $DIR/image/$newver/$bundle/randomfile
  rm $TMP
}

gen_file_plain() {
  local ver=$1
  local bundle=$2
  local name=$3

  # Add plain text file into a bundle
  mkdir -p $DIR/image/$ver/$bundle
  echo $name > $DIR/image/$ver/$bundle/$name
}

gen_file_plain_change() {
  local ver=$1
  local bundle=$2
  local name=$3

  # Add plain text file into a bundle
  mkdir -p $DIR/image/$ver/$bundle
  echo $ver $name > $DIR/image/$ver/$bundle/$name
}

# vi: ft=sh ts=8 sw=2 sts=2 et tw=80
