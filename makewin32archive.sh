#!/bin/bash

function errorExit
{
	echo $1
	exit 1
}

source version.inc

make -f Makefile.mxe clean
make -f Makefile.mxe || errorExit "Compilation error"

FILENAME=booterify_win32-$VERSION.zip

[ ! -f $FILENAME ] || errorExit "$FILENAME already exists!"

echo "Preparing $FILENAME"
zip -r $FILENAME *.exe *.c *.h Makefile* README.md bootsector.* pcjrloader.* version.inc bochs-config changelog.txt || errorExit "Error building archive. Missing file?"

echo "Done!"
ls -lh $FILENAME
