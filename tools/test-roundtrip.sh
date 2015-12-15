#!/bin/sh

set -ex

IN=$1
OUTF=$2
OUTP=$3

runtest() {
  local encArgs=$1
  local decArgs=$2

  ./flif $encArgs "${IN}" "${OUTF}"
  ./flif -d $decArgs ${OUTF} ${OUTP}
  test "`compare -metric AE ${IN} ${OUTP} null: 2>&1`" = "0"
}

runtest -I

runtest -N
