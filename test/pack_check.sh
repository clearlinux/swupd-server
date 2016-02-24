# Usage:
#         pack_check.sh buildversion [keep]
#         pack_check.sh 180
#         pack_check.sh 180 keep
#
# Download the user specified build version's Manifest.os-core and the pack
# from the prior build version.  Untar the file and output information on
# the largest files in the pack.
#
# If "keep" is specified the downloaded files will be left in a
# "pack_check" subdirectory for inspection.
#
# Note: this just gives a human a quick sense today of what's big in the pack
# and clues them in to an issue.  It needs built into a more detailed automatic
# test is need to insure that the pack has just one of either delta or full
# file for any file changed in the current build version.  That then is a
# post-pack-creation automated test that is run as part of the update build.

VER=$1
if [ -z "${VER}" ]; then
	echo "please provide a build version number, eg: \"180\""
	exit -1
fi

TEMPDIR=pack_check/$VER
mkdir -p ${TEMPDIR}
pushd ${TEMPDIR}

if ! [ -f Manifest.os-core.${VER} ]; then
	curl https://download.clearlinux.org/update/${VER}/Manifest.os-core > Manifest.os-core.${VER}
else
	echo "skipping download of Manifest.os-core.${VER}, already present"
fi
if ! [ -f pack-os-core-from-$((${VER}-10)).tar ]; then
	curl -O https://download.clearlinux.org/update/${VER}/pack-os-core-from-$((${VER}-10)).tar
else
	echo "skipping download of pack-os-core-from-$((${VER}-10)).tar, already present"
fi
tar xf pack-os-core-from-$((${VER}-10)).tar

rm -f biggest_files_names_${VER}.txt

ls -al --sort=size staged/ | grep -v total | head -n 20 > biggest_files_sizes_${VER}.txt
for hash in $(cat biggest_files_sizes_${VER}.txt | sed -e 's/^\(.*\) \([a-f0-9]*\)$/\2/')
do
	grep $hash Manifest.os-core.${VER} | grep -v -e "^.d.r" >> biggest_files_names_${VER}.txt
done

cat biggest_files_sizes_${VER}.txt | \
	sed -e 's/^\(.*\) \([0-9]*\) [A-F]\(.*\)/\2/' | \
	paste - biggest_files_names_${VER}.txt | column -t

popd
if [ "$2" != "keep" ]; then
	rm -rf ${TEMPDIR}
else
	echo "Leaving working files in ${TEMPDIR}"
fi
