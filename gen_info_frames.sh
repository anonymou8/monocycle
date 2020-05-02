#!/usr/bin/bash

# Run as:
#   ./gen_info_frames.sh

OUTPUT_DIR=info

FRAME=1

for bitrate in $(./exp_bitrate.py bitrate_only); do
    echo $FRAME
    sed "175 s/240/$bitrate/" info.svg > info2.svg
    rsvg-convert -d 96 info2.svg > ${OUTPUT_DIR}/info_$(printf '%06d' $FRAME).png
    FRAME=$((FRAME + 1))
done

rm info2.svg

