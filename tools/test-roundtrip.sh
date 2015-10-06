set -ex

IN=$1
OUTF=$2
OUTP=$3

./flif "${IN}" "${OUTF}"
./flif -d ${OUTF} ${OUTP}
test "`compare -metric AE ${IN} ${OUTP} null 2>&1`" = "0"
