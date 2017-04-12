#!/bin/sh

set -ex

FLIF=$1
IN=$2
OUTF=$3
OUTP=$4

EXIFIN=../tmp-test/in-exif.xmp
EXIFOUT=../tmp-test/out-exif.xmp
EXIFTOOL_OPTIONS="-XMP:*"

rm -f ${OUTF} ${OUTP} ${EXIFIN} ${EXIFOUT}
${FLIF} "${IN}" "${OUTF}"
${FLIF} -d ${OUTF} ${OUTP}
exiftool ${EXIFTOOL_OPTIONS} ${IN} -o ${EXIFIN}
exiftool ${EXIFTOOL_OPTIONS} ${OUTP} -o ${EXIFOUT}
diff ${EXIFIN} ${EXIFOUT}

