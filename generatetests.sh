#!/bin/sh

SRCDIR=testing/executables
LOGDIR=testing/logs
OUTDIR=testing/disks
OPTS="-f 360"

BOOTERIFY=./booterify

if [ ! -x $BOOTERIFY ]; then
	echo "Booterify not found. You should compile it first."
	exit 1
fi

for i in `ls $SRCDIR/*.EXE $SRCDIR/*.exe $SRCDIR/*.COM $SRCDIR/*.com`; do
	NM=`basename $i`

	echo -n Processing $NM...
	$BOOTERIFY bootsector.bin $i $OUTDIR/$NM.DSK $OPTS -B $LOGDIR/$NM.BRK > $LOGDIR/$NM.LOG
	if [ $? -ne 0 ]; then
		echo ERROR
		continue
	fi

	# There will be breakpoints if there were potential ints
	if [ -s $LOGDIR/$NM.BRK ]; then
		echo -n " Warning: Potential DOS int calls found: "
		wc -l $LOGDIR/$NM.BRK | cut -f 1 -d ' '
	else
		echo "OK"
	fi
done
