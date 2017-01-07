#!/bin/sh

set -ex

FLIF=$1
IN=$2
OUTF=$3
OUTP=$4

runtest() {
  local lossyArg=$1
  local orderArg=$2

  rm -f ${OUTF} ${OUTP}
  ${FLIF} $lossyArg $orderArg "${IN}" "${OUTF}"
  ${FLIF} -d ${OUTF} ${OUTP}
  test "`compare -metric AE ${IN} ${OUTP} null: 2>&1`" -lt 500000
}

runtest -I -Q40

runtest -I -Q0

runtest -N -Q40

runtest -N -Q0

