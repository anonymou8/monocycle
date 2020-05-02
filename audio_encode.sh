#!/usr/bin/bash

CHUNK_SIZE=4070     # in bytes

CHUNK=0

truncate -s 0 encoded.s16le

ffmpeg -i audio.wav -f s16le audio.s16le

for bitrate in $(./exp_bitrate.py bitrate_only); do
    echo $CHUNK : $bitrate
    dd if=audio.s16le of=chunk.s16le bs=${CHUNK_SIZE} count=1 skip=${CHUNK} 2> /dev/null
    oggenc -Q -r -C1 -R 48000 -b $bitrate -M $bitrate --advanced-encode-option lowpass_frequency=20 chunk.s16le 2> /dev/null
    ffmpeg -i chunk.ogg -f s16le - >> encoded.s16le 2> /dev/null
    CHUNK=$(( CHUNK + 1 ))
    truncate -s $(( CHUNK * CHUNK_SIZE )) encoded.s16le
done

ffmpeg -f s16le -ar 48000 -ac 1 -i encoded.s16le encoded.wav -y

# Although minimum bitrate was set to 32 kb/s 
# the actual bitrate of encoded chunks was 
# about 90 kb/s.