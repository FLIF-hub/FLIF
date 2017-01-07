#!/bin/sh

set -ex

IN=$1
OUTF=$2
OUTP=$3

runtest() {
  local lossyArg=$1
  local orderArg=$2

  rm -f ${OUTF} ${OUTP}
  ./flif $lossyArg $orderArg "${IN}" "${OUTF}"
  ./flif -d ${OUTF} ${OUTP}
  test "`compare -metric AE ${IN} ${OUTP} null: 2>&1`" -lt 500000
}

runtest -I -Q40

runtest -I -Q0

runtest -N -Q40

runtest -N -Q0

