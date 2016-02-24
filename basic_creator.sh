#!/bin/bash
VER=$1
XZ_DEFAULTS="--threads 0"
PREVREL=`cat /var/lib/update/image/latest.version`

SWUPDREPO=/usr/src/clear-projects/swupd-server
BUNDLEREPO=/usr/src/clear-projects/clr-bundles
UPDATEDIR=/var/lib/update

error() {
	echo "${1:-"Unknown Error"}"
	exit 1
}

# remove things from $BUNDLEREPO/bundles to make local build faster,
# eg: all of openstack, pnp, bat, cloud stuff, non-basic scripting language bundles
${SWUPDREPO}/mk_groups_ini.sh

export SWUPD_CERTS_DIR="/root/swupd-certs"
export LEAF_KEY="leaf.key.pem"
export LEAF_CERT="leaf.cert.pem"
export CA_CHAIN_CERT="ca-chain.cert.pem"
export PASSPHRASE="${SWUPD_CERTS_DIR}/passphrase"

${SWUPDREPO}/swupd_create_update --osversion ${VER}
${SWUPDREPO}/swupd_make_fullfiles ${VER}

pushd ${SWUPDREPO}

# create zero packs
MOM=${UPDATEDIR}/www/${VER}/Manifest.MoM
if [ ! -e ${MOM} ]; then
	error "no ${MOM}"
fi
BUNDLE_LIST=$(cat ${MOM} | awk -v V=${VER} '$1 ~ /^M\./ && $3 == V { print $4 }')
for BUNDLE in $BUNDLE_LIST; do
	#background them for parallelization
	${SWUPDREPO}/swupd_make_pack 0 ${VER} ${BUNDLE} &
done
#now await all completing
for job in $(jobs -p); do
	wait ${job}
	RET=$?
	if [ "$RET" != "0" ]; then
		error "zero pack subprocessor failed"
	fi
done

# create delta packs for 2 versions back
NUM_PACKS=2
${SWUPDREPO}/pack_maker.sh ${VER} ${NUM_PACKS}

popd ${SWUPDREPO}

# expose the new build to staging / testing
echo ${VER} > ${UPDATEDIR}/image/latest.version
STAGING=$(cat ${UPDATEDIR}/www//version/formatstaging/latest)
if [ "${STAGING}" -lt "${VER}" ]; then
	echo ${VER} > ${UPDATEDIR}/www//version/formatstaging/latest
fi

#valgrind /usr/src/clear-projects/swupd-server/swupd_create_update ${VER}
time hardlink /var/lib/update/image/${VER}/*
time hardlink /var/lib/update/image/$PREVREL /var/lib/update/image/${VER}
time hardlink /var/lib/update/www/$PREVREL /var/lib/update/www/${VER}
