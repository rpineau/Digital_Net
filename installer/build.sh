#!/bin/bash

PACKAGE_NAME="DigitalNet_X2.pkg"
BUNDLE_NAME="org.rti-zone.DigitalNetX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -s "$app_id_signature" ../build/Release/libDigitalNet.dylib
fi

mkdir -p ROOT/tmp/DigitalNet_X2/
cp "../DigitalNet.ui" ROOT/tmp/DigitalNet_X2/
cp "../focuserlist DigitalNet.txt" ROOT/tmp/DigitalNet_X2/
cp "../build/Release/libDigitalNet.dylib" ROOT/tmp/DigitalNet_X2/


if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}

else
    pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
