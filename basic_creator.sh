#!/bin/bash
VER=$1
XZ_DEFAULTS="--threads 0"

SWUPDREPO=${SWUPDREPO:-"/usr/src/clear-projects/swupd-server"}
BUNDLEREPO=${BUNDLEREPO:-"/usr/src/clear-projects/clr-bundles"}
UPDATEDIR=${UPDATEDIR:-"/var/lib/update"}

error() {
	echo "${1:-"Unknown Error"}"
	exit 1
}

# remove things from $BUNDLEREPO/bundles to make local build faster,
# eg: all of openstack, pnp, bat, cloud stuff, non-basic scripting language bundles
${SWUPDREPO}/mk_groups_ini.sh

PREVREL=`cat ${UPDATEDIR}/image/latest.version`

export SWUPD_CERTS_DIR=${SWUPD_CERTS_DIR:-"/root/swupd-certs"}
export LEAF_KEY="leaf.key.pem"
export LEAF_CERT="leaf.cert.pem"
export CA_CHAIN_CERT="ca-chain.cert.pem"
export PASSPHRASE="${SWUPD_CERTS_DIR}/passphrase"

${SWUPDREPO}/swupd_create_update --osversion ${VER} --statedir ${UPDATEDIR}
${SWUPDREPO}/swupd_make_fullfiles --statedir ${UPDATEDIR} ${VER}

# create zero packs
MOM=${UPDATEDIR}/www/${VER}/Manifest.MoM
if [ ! -e ${MOM} ]; then
	error "no ${MOM}"
fi
BUNDLE_LIST=$(cat ${MOM} | awk -v V=${VER} '$1 ~ /^M\./ && $3 == V { print $4 }')
for BUNDLE in $BUNDLE_LIST; do
	#background them for parallelization
	${SWUPDREPO}/swupd_make_pack --statedir ${UPDATEDIR} 0 ${VER} ${BUNDLE} &
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

# expose the new build to staging / testing
echo ${VER} > ${UPDATEDIR}/image/latest.version

STAGING_FILE="${UPDATEDIR}/www/version/formatstaging/latest"
if [ ! -f "${STAGING_FILE}" ]; then
	mkdir -p "${UPDATEDIR}/www/version/formatstaging"
	STAGING=0
	echo ${STAGING} > ${UPDATEDIR}/www/version/formatstaging/latest
else
	STAGING=$(cat ${STAGING_FILE})
fi

if [ "${STAGING}" -lt "${VER}" ]; then
	echo ${VER} > ${UPDATEDIR}/www/version/formatstaging/latest
fi

#valgrind ${SWUPDREPO}/swupd_create_update --osversion ${VER} --statedir ${UPDATEDIR}
time hardlink ${UPDATEDIR}/image/${VER}/*
time hardlink ${UPDATEDIR}/image/${PREVREL} ${UPDATEDIR}/image/${VER}
time hardlink ${UPDATEDIR}/www/${PREVREL} ${UPDATEDIR}/www/${VER}
