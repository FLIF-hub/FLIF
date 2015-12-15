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

IN=$1
OUTF=$2

runtest() {
  local encArgs=$1
  local decArgs=$2

  ./flif $encArgs test-frame*.png "${OUTF}"
  ./flif -d $decArgs ${OUTF} decoded-test-frame.pam
  check test-frame decoded-test-frame*.pam
  rm decoded-test-frame*.pam
}


convert -coalesce "${IN}" test-frame%04d.png

# test a few combinations of encode parameters
runtest -I
runtest -NL0
runtest -SP0
runtest -L50

rm test-frame*.png
