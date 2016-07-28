#!/bin/sh

SRCDIR=testing/executables
LOGDIR=testing/logs
OUTDIR=testing/disks
OPTS="-s 9 -t 368640"

for i in `ls $SRCDIR/*.EXE $SRCDIR/*.exe $SRCDIR/*.COM $SRCDIR/*.com`; do
	NM=`basename $i`

	echo -n Processing $NM...
	./booterify bootsector.bin $i $OUTDIR/$NM.DSK $OPTS -B $LOGDIR/$NM.BRK > $LOGDIR/$NM.LOG
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
