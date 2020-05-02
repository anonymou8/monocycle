### This bunch of scripts can make *color spectral images* from video frames by 

- converting each color channel of a frame to an audible waveform,
- then encoding audio with an audio encoder of a choice,
- then taking spectral "waterfall" image of the encoded audio,
- then combining the spectrums back to an image.

<img src="https://raw.githubusercontent.com/anonymou8/monocycle/master/frame_0333.jpg" style="width:100%"/>

### Files description
```
    frames/                         frames source folder
    spectrums/                      spectrums target folder
    info/                           information frames target folder (from `gen_info_frames.sh`)
    
    exp_bitrate.py                  generates pairs of 'file' 'bitrate' for the frames
    generate_color_spectrum.sh      generates one color spectrum image at a time, takes 'file' and 'bitrate' as parameters
    gen_info_frames.sh              generates info frames for all the images in a folder
    audio_encode.sh                 encodes audio with increasing bitrate; it does it wrong :(
    
    info.svg                        info frame template; there's second hidden layer for a title frame
    
    enscribe.c                      C program by Jason Downer for converting images to waveforms
    monocycle.c                     C program for converting waveforms to spectral images
    Makefile                        a Makefile for `enscribe` and `monocycle`; run `make`
```

### Dependencies

```
    FFmpeg                          generate frames and play audio waves
    ImageMagick                     separate and combine image channels, resize images
    Python >= 3.6                   generate exponential bitrate values
    GCC                             compile C sources
    libs:
        gd
        png
        jpeg
        sndfile
        fftw3
    make    [optional]
    librsvg [optional]              `rsvg-convert` for info frames
```

### The algorithm is like:

**Before you run this, take a look inside the scripts
and configure them properly for your setup. There are
magic numbers you must change.**

```bash
    #!/usr/bin/bash
    
    mkdir frames spectrums info
    ffmpeg -i movie.mp4 frames/frame_%06d.png
    
    make
    ./exp_bitrate.py | xargs -n2 -P8 ./generate_color_spectrum.sh
    ./gen_info_frames.sh
```

And here you have them: a lot of images of different formats 
that you want to use as frames in your favorite video editor. 
I use [Blender](https://www.blender.org/).

*`generate_color_spectrum.sh` conversion speed (for me) is about 
15 seconds per FullHD frame per CPU core with Vorbis encoder.*

