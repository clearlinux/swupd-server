#! /bin/bash

COMMON=${COMMON:-"/root/common"}
BUNDLEREPO=${BUNDLEREPO:-"/usr/src/clear-projects/clr-bundles"}

UPDATEDIR=${UPDATEDIR:-"/var/lib/update"}
SWUPD_GROUPS_INI=${SWUPD_GROUPS_INI:-"$UPDATEDIR/groups.ini"}
SWUPD_SERVER_INI=${SWUPD_SERVER_INI:-"$UPDATEDIR/server.ini"}

if [ ! -d "$UPDATEDIR" ]; then
	mkdir -p $UPDATEDIR/{image,www}
fi

if [ ! -f "$SWUPD_SERVER_INI" ]; then
	template=$(echo $BASH_SOURCE)
	template=$(dirname $template)
	cp -p $template/server.ini $SWUPD_SERVER_INI
	sed -i "s|/var/lib/update|$UPDATEDIR|" $SWUPD_SERVER_INI
fi

if [ ! -f "$UPDATEDIR/image/latest.version" ]; then
	echo "0" > $UPDATEDIR/image/latest.version
fi

echo "rebuilding $SWUPD_GROUPS_INI based on $BUNDLEREPO"
rm -f $SWUPD_GROUPS_INI

for bundle in $(ls $BUNDLEREPO/bundles)
do
	echo "[$bundle]" >> $SWUPD_GROUPS_INI
	echo "group=$bundle" >> $SWUPD_GROUPS_INI
	echo "" >> $SWUPD_GROUPS_INI
done
