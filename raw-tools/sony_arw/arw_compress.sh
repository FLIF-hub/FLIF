#! /bin/bash

set -e

SRC="$1"
DST="$2"

if [ "x$SRC" == "x" ]
then
	echo "Need a source file!"
	exit 1
fi

if [ "x$DST" == "x" ]
then
	DST="$SRC.flifraw"
fi

if [ -e "$DST" ]
then
	echo "Destination $DST already exists, refusing to overwrite"
	exit 1
fi

DIR=$(mktemp -d "$DST.XXXXXX")

sha256sum - < "$SRC" > "$DIR"/orig.sha256
dcraw_hack "$SRC" > "$DIR"/temp.rgba 2> "$DIR"/err
HALF_SIZE=$(head -n1 "$DIR"/err | sed -e 's/^.*, half=\([0-9]*x[0-9]*\)$/\1/')
convert -depth 16 -size $HALF_SIZE rgba:"$DIR"/temp.rgba png:"$DIR"/temp.png
grep ALARM "$DIR"/err | cut -c 8- | xxd -r -ps > "$DIR"/alarms.bin
START=$(head -n1 "$DIR"/err | sed -e 's/^.*data_at=//' -e 's/, .*$//')
HALF_WIDTH=$(echo $HALF_SIZE | sed -e 's/x.*$//')

flif -e --keep-invisible-rgb "$DIR"/temp.png "$DIR"/result.flif

flif -d "$DIR"/result.flif "$DIR"/result.flif.png
head -c $START "$SRC" > "$DIR"/begin

if (cat "$DIR"/begin; convert "$DIR"/result.flif.png -depth 16 rgba:- | arw_encode $HALF_WIDTH $START "$DIR"/alarms.bin) | cmp - "$SRC"
then
	tar --transform 's=^.*/==' -Jcf "$DIR"/{extra.tar.xz,alarms.bin,begin,orig.sha256}
	tar --transform 's=^.*/==' -cf "$DST" "$DIR"/extra.tar.xz "$DIR"/result.flif
	rm "$DIR"/{extra.tar.xz,result.flif,alarms.bin,begin,err,result.flif.png,temp.png,orig.sha256,temp.rgba}
	rmdir "$DIR"
else
	echo data mismatch, temp stuff left in $DIR
	exit 1
fi
