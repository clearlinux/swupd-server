#! /bin/bash

COMMON=${COMMON:-"/root/common"}
BUNDLEREPO=${BUNDLEREPO:-"/root/clr-bundles"}

SWUPD_SERVER_DIR=${SWUPD_SERVER_DIR:-"/var/lib/update"}
SWUPD_GROUPS_INI=${SWUPD_GROUPS_INI:-"$SWUPD_SERVER_DIR/groups.ini"}
SWUPD_SERVER_INI=${SWUPD_SERVER_INI:-"$SWUPD_SERVER_DIR/server.ini"}

if [ ! -d "$SWUPD_SERVER_DIR" ]; then
	mkdir -p $SWUPD_SERVER_DIR/{image,www}
fi

if [ ! -f "$SWUPD_SERVER_INI" ]; then
	template=$(echo $BASH_SOURCE)
	template=$(dirname $template)
	cp -p $template/server.ini $SWUPD_SERVER_INI
fi

if [ ! -f "$SWUPD_SERVER_DIR/image/latest.version" ]; then
	echo "0" > $SWUPD_SERVER_DIR/image/latest.version
fi

echo "rebuilding $SWUPD_GROUPS_INI based on $BUNDLEREPO"
rm -f $SWUPD_GROUPS_INI

for bundle in $(ls $BUNDLEREPO/bundles)
do
	echo "[$bundle]" >> $SWUPD_GROUPS_INI
	echo "group=$bundle" >> $SWUPD_GROUPS_INI
	echo "" >> $SWUPD_GROUPS_INI
done
