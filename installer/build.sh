#!/bin/bash

mkdir -p ROOT/tmp/DigitalNet_X2/
cp "../DigitalNet.ui" ROOT/tmp/DigitalNet_X2/
cp "../focuserlist DigitalNet.txt" ROOT/tmp/DigitalNet_X2/
cp "../build/Release/libDigitalNet.dylib" ROOT/tmp/DigitalNet_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.DigitalNet_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 DigitalNet_X2.pkg
pkgutil --check-signature ./DigitalNet_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.DigitalNet_X2 --scripts Scripts --version 1.0 DigitalNet_X2.pkg
fi

rm -rf ROOT
