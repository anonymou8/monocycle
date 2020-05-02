#!/usr/bin/bash

# Run as:
#   mkdir spectrums
#   ./exp_bitrate.py | xargs -n2 -P8 ./generate_color_spectrum.sh
#
# JPEGs are used for speed and size, PNGs are too huge.

OUTPUT_DIR=spectrums

INPUT=$1
BITRATE=$2
FILENAME=$(basename $INPUT)
OUTPUT=${OUTPUT_DIR}/${FILENAME%.*}.jpg
ORIGINAL_WIDTH=$(identify -format %w $INPUT)

echo $INPUT : $BITRATE

mkdir $FILENAME

convert $INPUT -separate -quality 100% ${FILENAME}/chan_%d.jpg

cd $FILENAME
    for c in chan_{0,1,2}.jpg; do
        ./enscribe -ts=2 -hf=90.7 -length=103.52 $c ${c%.*}.wav 2> /dev/null
        
        # CHANGE AUDIO ENCODER HERE
        oggenc -Q -b $BITRATE --advanced-encode-option lowpass_frequency=20 ${c%.*}.wav 2> /dev/null
                                       ###
        ffmpeg -hide_banner -i ${c%.*}.ogg -ar 44100 -f f32le - 2> /dev/null \
        | ./monocycle --push ${c%.*}_spectrum.png -s 4096 -b 1024 -B 249 | grep Max
    done
cd ..

convert ${FILENAME}/chan_{0,1,2}_spectrum.png -combine -filter sinc -resize 50% \
    -resize $ORIGINAL_WIDTH -quality 91% $OUTPUT

rm -r $FILENAME
#~ rm $INPUT           # CAUTION! Removes original frame!
