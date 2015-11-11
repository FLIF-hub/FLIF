#!/bin/bash

FLIF_FLAGS=""

make clean

git reset --hard HEAD > times.log
if [ ! -d "./test_images" ] ; then
	echo "Need directory named 'test_images' to contain sample images."
	exit 1
fi

# if "old" already exists for some reason, do NOT work on current branch but bail out
git checkout -b old || exit 2



for i in `seq 1 50`; do
    git_head=`git log -1 --format=%h`
    # if build fails, make clean and try again
    make flif CXXFLAGS="-O3" -j4 || make clean ; make flif CXXFLAGS="-O3" -j4 || (git reset --hard HEAD^1 ; continue)
    echo "Run number $i"
    mkdir -p test_images/run_${i}
    cd test_images
	for image in `find . | grep '\.png$'` ; do
		../flif ${FLIF_FLAGS} ${image} run_${i}/${image//png/flif}
	done
	cd ..
	echo "run $i `du -sb test_images/run_${i} | cut -d' ' -f1` $git_head "  | tee -a sizes.log
    git reset --hard HEAD^1
done
