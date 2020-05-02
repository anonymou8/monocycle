/*
    Compile with: 
        gcc -lm -lfftw3 -lpng monocycle.c -o monocycle
        
    Run ./monocycle without arguments for help.
*/

/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <float.h>

#include <fftw3.h>
#include <png.h>

typedef unsigned int uint;

uint N      = 2048;             // FFT length
uint FREQ   = 44100;            // samplerate
uint L_FREQ = 20;               // lower frequency
uint U_FREQ = 20000;            // upper frequency
float LIGHTNESS = 1.0;          // image lighness
float NOISE_GATE = FLT_MIN;     // values below this level will be cut off
int INVERT_COLORS = 0;          // invert colors
int NORMALIZE = 0;              // scale color values to full image range
int EVEN_SIZE = 0;              // make image dimensions divisible by 2

uint WIDTH = 0;                 // image width
uint HEIGHT;                    // image height, N/2
int WRITE_FLOATS = 0;           // whether output PNG or raw float values
uint SKIP_SMPLES;               // skip that much samples at the begining;
                                // half of input buffer length gives 
                                // pretty sharp result

uint L_INDEX;                   // index of lower frequency
uint U_INDEX;                   // index of upper frequency

float *INPUT_BUFFER;
uint INPUT_BUFFER_LENGTH = 512;
void  *IMAGE;                   // image data

#define REASONABLE_MEMORY_LIMIT_FOR_INPUT_BUFFER 254016000  // in bytes

#define MAX_IMAGE_WIDTH 32768   // although PNG can be as large as 2^31-1 pixels, 
                                // no apps I know can even handle dimensions of 32k
void *POINTERS[MAX_IMAGE_WIDTH];      // pointers to image columns or rows

double *FFT_IN, *FFT_IN_PREV;
fftw_complex *FFT_OUT;
fftw_plan FFTW_PLAN;
double *WINDOW_FUNC;

void wf_blackman_harris() {
    for (int i=0; i<N; i++) {
        WINDOW_FUNC[i] = 0.35875 - 0.48829*cos(2*M_PI * i / (N-1)) \
                                 + 0.14128*cos(4*M_PI * i / (N-1)) \
                                 - 0.01168*cos(6*M_PI * i / (N-1));
    }
}

void allocate_vars() {
    L_INDEX = (double)L_FREQ/FREQ * (N-1);
    U_INDEX = (double)U_FREQ/FREQ * (N-1);
    HEIGHT  = U_INDEX - L_INDEX + 1;

    FFT_IN      = (double*) fftw_malloc(N * sizeof(double));
    FFT_IN_PREV = (double*) calloc(N, sizeof(double));
    FFT_OUT     = (fftw_complex*) fftw_malloc((1+N/2) * sizeof(fftw_complex));
    FFTW_PLAN   = fftw_plan_dft_r2c_1d(N, FFT_IN, FFT_OUT, FFTW_ESTIMATE);
    
    WINDOW_FUNC = (double*) malloc(N * sizeof(double));
    wf_blackman_harris();
    
    INPUT_BUFFER = (float*) malloc(INPUT_BUFFER_LENGTH * sizeof(float));
}

void free_vars() {
    fftw_free(FFT_IN);
    fftw_free(FFT_IN_PREV);
    fftw_free(FFT_OUT);
    fftw_destroy_plan(FFTW_PLAN);
    
    free(WINDOW_FUNC);
    free(INPUT_BUFFER);
    
    free(IMAGE);                                                    // free 2
}

int do_one_stripe() {
    int samples_read, samples_left;
    
    samples_read = fread(INPUT_BUFFER, sizeof(float), INPUT_BUFFER_LENGTH, stdin);
    samples_left = N - samples_read;
    if (samples_left < 0) { samples_left = 0; }

    for (int i=0; i<samples_left; i++) {
        FFT_IN[i]  = FFT_IN_PREV[samples_read+i];
    }
    for (int i=samples_left; i<N; i++) {
        FFT_IN[i]  = INPUT_BUFFER[i-samples_left];
    }
    
    for (int i=0; i<N; i++) {
        FFT_IN_PREV[i] = FFT_IN[i];
        FFT_IN[i] *= WINDOW_FUNC[i];
    }
    
    fftw_execute(FFTW_PLAN);
    
    float mag, amp, luma;
    float *column;
    
    column = (float*) calloc(HEIGHT, sizeof(float));                // allocate 1

    for (int i=L_INDEX; i<U_INDEX; i++) {
        mag = hypot(FFT_OUT[i][0], FFT_OUT[i][1]);
        
        /* The value of amplitude is not true, but I like how it looks.
           And because it is not true, the NOISE_GATE controls an abstract 
           value, not the real amplitude.
        */
        amp = sqrt(mag / sqrt(2*N) / 2) / 2;
        
        if (amp < NOISE_GATE) {
            luma = 0; 
        } else {
            luma = amp * pow(10, LIGHTNESS - 1);
        }
        
        if (INVERT_COLORS) { luma = 1 - luma; }
        
        column[HEIGHT-1 - (i-L_INDEX)] = luma;
    }
    
    POINTERS[WIDTH] = column;
    WIDTH++;
    
    if (feof(stdin)) {
        WIDTH--;
        return 0;
    }
    
    return 1;
}

void compose_image() {
    //~ uint16_t *image_png;
    uint8_t *image_png;
    float *image_float;
    uint16_t p, pmaxv=0, pminv=0xffff;
    float pf, pfmaxv=-FLT_MAX, pfminv=FLT_MAX;
    float norm_scale=1, norm_shift=0;
    
    float **pointers = (float**)POINTERS;
    
    if (EVEN_SIZE) {
        WIDTH = (~1) & WIDTH;
        HEIGHT = (~1) & HEIGHT;
    }
    
    if (WRITE_FLOATS) {                                             // allocate 2
        image_float = (float*) calloc(WIDTH*HEIGHT, sizeof(float));
    } else {
        //~ image_png = (uint16_t*) calloc(WIDTH*HEIGHT, sizeof(uint16_t));
        image_png = (uint8_t*) calloc(WIDTH*HEIGHT, sizeof(uint8_t));
    }
    
    if (NORMALIZE) {
        for (int y=0; y<HEIGHT; y++) {
            for (int x=0; x<WIDTH; x++) {
                pf = pointers[x][y];
                if (pf > pfmaxv) { pfmaxv = pf; }
                if (pf < pfminv) { pfminv = pf; }
            }
        }
        norm_scale = 1 / (pfmaxv - pfminv);
        norm_shift = -pfminv;
        pfminv = FLT_MAX;
        pfmaxv = -FLT_MAX; 
    }
    
    for (int y=0; y<HEIGHT; y++) {
        for (int x=0; x<WIDTH; x++) {
            pf = (pointers[x][y] + norm_shift) * norm_scale;
            if (WRITE_FLOATS) {
                image_float[x + y*WIDTH] = pf;
                if (pf > pfmaxv) { pfmaxv = pf; }
                if (pf < pfminv) { pfminv = pf; }
            } else {
                // Overflow? Never heared of it.
                //~ p = 0xffff  * pf;
                p = 0xff  * pf;
                image_png[x + y*WIDTH] = p;
                if (p > pmaxv) { pmaxv = p; }
                if (p < pminv) { pminv = p; }
            }
        }
    }
    
    if (WRITE_FLOATS) {
        printf("Min pixel value: %f\n", pfminv);
        printf("Max pixel value: %f\n", pfmaxv);
    } else {
        printf("Min pixel value: %04x\n", pminv);
        printf("Max pixel value: %04x\n", pmaxv);
    }
    
    for (int i=0; i<WIDTH; i++) {
        free(POINTERS[i]);                                          // free 1
    }
    
    if (WRITE_FLOATS) {
        IMAGE = image_float;
    } else {
        for (int i=0; i<HEIGHT; i++) {
            POINTERS[i] = &image_png[i*WIDTH];
        }
        IMAGE = image_png;
    }
}

int save_as_floats(char* filename) {
    FILE *f = fopen(filename, "w");
    
    if (f == NULL) { return 0; }
    
    fwrite(IMAGE, sizeof(float)*WIDTH, HEIGHT, f);
    
    fclose(f);
    return 1;
}

int save_as_png(char* filename) {
    int rv = 1;
    FILE *f = fopen(filename, "w");
    
    if (f == NULL) { return 0; }
    
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(f); return 0; }
    png_infop info = png_create_info_struct(png);
    if (!info) { rv = 0; goto finally; }
    
    if (setjmp(png_jmpbuf(png))) { rv = 0; goto finally; }
    
    png_init_io(png, f);
    //~ png_set_IHDR(png, info, WIDTH, HEIGHT, 16, PNG_COLOR_TYPE_GRAY,
    png_set_IHDR(png, info, WIDTH, HEIGHT, 8, PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png, info);
    png_set_swap(png);
    png_write_image(png, (png_byte**)POINTERS);
    png_write_end(png, NULL);
    
finally:
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return rv;
}

void usage(int exit_code) {
    puts("Usage: monocycle [options] image\n\n"
    "   Monocycle reads native endian floats from stdin and uses\n"
    "   FFT to convert them to monochrome spectrum image. Sample\n"
    "   rate determines only frequency range and not the image size.\n"
    "   Image size can be estimated as `samples/bufsize` x `fftsize/2`.\n"
    "   `image` is the image filename, e.g. `image.f32`. Be careful,\n"
    "   file with the name will be overwritten with no warnings!\n\n"
    "   Options:\n"
    "       -s, --fftsize n         - FFT buffer size, default=2048\n"
    "       -b, --bufsize n         - input buffer size, default=512\n"
    "       --skip n                - skip n samples at the beginning, default=bufsize/2\n"
    "       -r, --rate n            - input data sample rate, default=44100\n"
    "       -l, --lower-freq n      - lower frequency limit, default=20 Hz\n"
    "       -u, --upper-freq n      - upper frequency limit, default=20000\n"
    "       -B, --brightness n      - image brightness = pow(10, n/100-1), default=100.0\n"
    "       -g, --gate n            - noise gate in decibels full scale, default=-∞\n"
    "       -n, --normalize         - scale pixels brightness to full range\n"
    "       -i, --invert            - invert image colors\n"
    "       -f, --floats            - output to raw floats instead of PNG;\n"
    "                                 image size will be printed to stdout.\n"
    "       -2                      - make image dimensions divisible by 2.\n"
    "       -h, --help              - print this message.\n\n"
    "   Example for x86-compatible machine:\n"
    "       ffmpeg -i wave.mp3 -f f32le - | monocycle spectrum.png\n"
    "   \n"
    );
    
    exit(exit_code);
}

void parse_args(int argc, char* argv[]) {
    const struct option longopts[] = {
        {"fftsize",     required_argument, NULL, 's'},
        {"bufsize",     required_argument, NULL, 'b'},
        {"skip",        required_argument, NULL, 'S'},
        {"rate",        required_argument, NULL, 'r'},
        {"lower-freq",  required_argument, NULL, 'l'},
        {"upper-freq",  required_argument, NULL, 'u'},
        {"brightness",  required_argument, NULL, 'B'},
        {"gate",        required_argument, NULL, 'g'},
        {"normalize",   no_argument,       NULL, 'n'},
        {"invert",      no_argument,       NULL, 'i'},
        {"floats",      no_argument,       NULL, 'f'},
        {"help",        no_argument,       NULL, 'h'},
        {"push",        no_argument,       NULL, '8'},
    };
    
    int longindex, opt, have_to_push = 1;
    int has_skip_option = 0;
    
    while ((opt = getopt_long(argc, argv, "s:b:r:l:u:B:g:nif2h", longopts, &longindex)) != -1) {
        switch (opt) {
            case '?':
                usage(1);
            case 'h':
                usage(0);
            case '8':
                have_to_push = 0;
                break;

            case 's':
                N = atoi(optarg);
                break;
            case 'b':
                INPUT_BUFFER_LENGTH = atoi(optarg);
                break;
            case 'S':
                SKIP_SMPLES = atoi(optarg);
                has_skip_option = 1;
                break;
            case 'r':
                FREQ = atoi(optarg);
                break;
            case 'l':
                L_FREQ = atoi(optarg);
                break;
            case 'u':
                U_FREQ = atoi(optarg);
                break;
            case 'B':
                LIGHTNESS = atof(optarg) / 100;
                break;
            case 'g':
                NOISE_GATE = pow(10, atof(optarg)/20);
                break;
            case 'n':
                NORMALIZE = 1;
                break;
            case 'i':
                INVERT_COLORS = 1;
                break;
            case 'f':
                WRITE_FLOATS = 1;
                break;
            case '2':
                EVEN_SIZE = 1;
                break;
            default:
                break;
        }
    }
    
    if (optind >= argc) {
        fputs("ERROR: No output file specified.\n", stderr);
        usage(2);
    }
    if (INPUT_BUFFER_LENGTH == 0 || INPUT_BUFFER_LENGTH*sizeof(float) > REASONABLE_MEMORY_LIMIT_FOR_INPUT_BUFFER) {
        fputs("ERROR: Buffer length is too smart.\n", stderr);
        exit(42);
    }
    if (!has_skip_option) { 
        SKIP_SMPLES = INPUT_BUFFER_LENGTH / 2; 
    }
    if (U_FREQ > FREQ / 2) {
        fputs("ERROR: Upper frequency cannot exceed half of sampling frequency.\n", stderr);
        exit(3);
    }
    if (L_FREQ >= U_FREQ) {
        fputs("ERROR: Lower frequency must be lower than upper frequency.\n", stderr);
        exit(4);
    }
    if (have_to_push) {
        fputs("This monocycle has no pedals. You have to --push it.\n", stderr);
        exit(8);
    }
}

int skip_smples(int skip) {
    if (skip) {
        if (INPUT_BUFFER_LENGTH < SKIP_SMPLES) {
            uint reads = SKIP_SMPLES / INPUT_BUFFER_LENGTH;
            uint last_read_size = SKIP_SMPLES % INPUT_BUFFER_LENGTH;
            while (reads--) {
                fread(INPUT_BUFFER, sizeof(float), INPUT_BUFFER_LENGTH, stdin);
                if (feof(stdin)) { return 0; }
            }
            fread(INPUT_BUFFER, sizeof(float), last_read_size, stdin);
        } else {
            fread(INPUT_BUFFER, sizeof(float), SKIP_SMPLES, stdin);
        }
    }
    
    if (feof(stdin)) { return 0; }
    return 1;
}

int main(int argc, char* argv[]) {
    char *filename;
    
    parse_args(argc, argv);
    filename = argv[optind];
    
    allocate_vars();
    
    if (!skip_smples(SKIP_SMPLES)) {
        fputs("No samples - no work.\n", stderr);
        free_vars();
        exit(5);
    }
    
    while (do_one_stripe()) {};
    
    compose_image();
    
    int ok;
    
    if (WRITE_FLOATS) {
        ok = save_as_floats(filename);
    } else {
        ok = save_as_png(filename);
    }
    
    if (ok) {
        printf("Image size: %ux%u\n", WIDTH, HEIGHT);
    } else {
        fputs("Error writing file.\n", stderr);
    }
    
    free_vars();
}
