#!/bin/bash

# compare $1 (base name without .png extension, could be multiple frames) one by one to the rest of the arguments
function check {
        one=$1*.png
        for c in $one
        do
                    if [[ $(compare -metric mepp $c $2 null: 2>&1) == "0 (0, 0)" ]]
                    then
                      #echo "OK-compare (identical decoded images)"
                      shift
                      continue
                    else
                      echo "BUG DETECTED!!!"
                      echo "PROBLEM IN FILE $1 : $c is not the same image as $2"
                      exit 1
                    fi
        done
}


set -ex

FLIF=$1
IN=$2
OUTF=$3

runtest() {
  local encArgs=$1
  local decArgs=$2

  rm -f ${OUTF} decoded-test-frame*.pam
  $FLIF -vv $encArgs ${IN}/*.png "${OUTF}"
  $FLIF -vv -d $decArgs ${OUTF} decoded-test-frame.pam
  check ${IN}/ decoded-test-frame*.pam
  rm decoded-test-frame*.pam
}



# test a few combinations of encode parameters
runtest -I
runtest -NL0
runtest -SP0
runtest -L50

