#!/bin/bash

# Update content is created and output into the www directory, but the
# published "latest" version number is only bumped in
# www/version/formatstaging/latest.  This enables QA on devices whose update
# client is set to query the www/version/formatstaging/latest.  Once that
# QA is complete, this publish script is run to bump the production
# format's "latest" version number.

if [[ $# < 1 ]]; then
    echo "First argument must be a format number"
    echo "$0 aborting"
    exit 1
fi
FORMAT=$1
UPDATEDIR=${UPDATEDIR:-"/var/lib/update"}

ver1=`cat ${UPDATEDIR}/www/version/formatstaging/latest`
ver2=`cat ${UPDATEDIR}/www/version/format$FORMAT/latest`
cp ${UPDATEDIR}/www/version/formatstaging/latest ${UPDATEDIR}/www/version/format$FORMAT/latest
echo "Updated from $ver2 to $ver1 in format $FORMAT"
