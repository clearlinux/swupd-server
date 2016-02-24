#! /bin/bash

# This creates a bunch of files, all with the same content, and with various
# combinations of xaddrs. It lists them with their inode numbers, then runs
# hardlink on them, and then lists them with inodes again. Since name of each
# file corresponds to is xattrs, you can easily see if expected linking is
# indeed happening.

TMPDIR=$(mktemp -d /tmp/hardlinktest-XXXXXX)

makeFile() {
    local argC=$#
    local fName=${1}-File
    shift 1; (( argC = $argC - 1 ))
    local nameValPairs=$*
    while (( $argC >= 1 )) ; do
        if (( $argC == 1 )) ; then
            fName=${fName}-${1}
            shift 1; (( argC = $argC - 1 ))
        else
            if [[ -z $2 ]] ; then
                fName=${fName}-${1}
            else
                fName=${fName}-${1}:${2}
            fi
            shift 2; (( argC = $argC - 2 ))
        fi;
    done
    echo "must have some content else can't set xattrs" >  $TMPDIR/$fName
    setXattrs $fName $nameValPairs
}

setXattrs() {
    local argC=$#
    local fName=${1}
    shift 1; (( argC = $argC - 1 ))
    local nameValPairs=$*
    while (( $argC >= 1 )) ; do
        if (( $argC == 1 )) ; then
            setfattr -n user.$1 $TMPDIR/$fName
            shift 1; (( argC = $argC - 1 ))
        else
            if [[ -z $2 ]] ; then
                setfattr -n user.$1 $TMPDIR/$fName
            else
                setfattr -n user.$1 -v $2 $TMPDIR/$fName
            fi
            shift 2; (( argC = $argC - 2 ))
        fi;
    done
}

for prefix in A B ; do
      makeFile $prefix

      makeFile $prefix XXX XXX
      makeFile $prefix XXX

      makeFile $prefix XXX XXX YYY YYY
      makeFile $prefix YYY YYY XXX XXX
      makeFile $prefix XXX ""  YYY YYY
      makeFile $prefix XXX XXX YYY
      makeFile $prefix XXX ""  YYY
      makeFile $prefix YYY ""  XXX

      makeFile $prefix XXX XXX YYY YYY ZZZ ZZZ
      makeFile $prefix YYY YYY XXX XXX ZZZ ZZZ
      makeFile $prefix ZZZ ZZZ YYY YYY XXX XXX

      makeFile $prefix XXX XXXXXXXXXX YYY YYYYYYYY ZZZ ZZZ
      makeFile $prefix YYY YYYYYYYY XXX XXXXXXXXXX ZZZ ZZZ
      makeFile $prefix ZZZ ZZZ YYY YYYYYYYY XXX XXXXXXXXXX

     makeFile $prefix AA V BB V CC V DD V
     makeFile $prefix AAAAAAAAAA V BBBBBBBBBB V
done

pushd $TMPDIR > /dev/null
getfattr -d *
ls -1i | sort
popd > /dev/null

hardlink $TMPDIR --ignore-time --respect-xattrs --maximize

pushd $TMPDIR > /dev/null
ls -1i | sort
popd > /dev/null

rm -rf $TMPDIR
