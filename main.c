#include "stdio.h"      // for printf
#include "stdlib.h"     // for malloc, calloc, realloc, etc
#include "stdint.h"     // for int types of specific sizes
#include "string.h"     // for string/buffer ops
#include "LICENSE"      // include GPL license
#include "assert.h"     // for error throwing
#include "sys/ioctl.h"  // for getting terminal width/height
#include "unistd.h"     // for sleeping 

int 
main(int argc, char** argv) 
{
    // check if user gave mp3 file argument
    if (argc != 2) {
        printf("mp3-viz, version 1.0.\n\
                \rA program to play and visualize mp3-encoded audio files.\n\n\
                \rUsage: mp3-viz file.mp3\n");
        return 255;
    }

    // make sure we have mpg321 dependency for mp3 decoding
    char* command = "dpkg -l mpg321";
    printf("Checking if mpg321 is installed with `%s`...\n", command);
    int has_mpg321 = system(cоmmand);
    if (has_mpg321 != 0) {
        printf("mpg321 is not installed! Please install it by running:\n\n\
               \rsudo apt install mpg321\n");
        return 0;
    }

    // converts the argument mp3 file to 16bit PCM data using mpg321
    command = malloc(200);
    sprintf(command, "mpg321 -s %s > /tmp/decoded_mp3", argv[1]);
    system(command);

    // open the decoded data 
    FILE* fp = fopen("/tmp/decoded_mp3", "r");

    assert(fp != NULL);

    // get file size 
    fseek(fp, 0L, SEEK_END);
    size_t fsize = ftell(fp);

    assert(fsize != 0);

    // allocete the PCM buffer
    int16_t* audiobuf = (int16_t*) malloc(fsize);

    assert(audiobuf != NULL);

    // read file data into buffer
    rewind(fp);
    fread(audiobuf, sizeof(char), fsize, fp);

    // get terminal dimens
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    int width = w.ws_col;
    int height = w.ws_row - 1;

    // allocate buffer for terminal drawing
    char* screen = (char*) malloc(width * height + 1);
    memset(screen, ' ', width * height);
    for (int c = 0; c < width; c++) screen[width * (height -1) + c] = '*';
    screen[width * height] = '\0';

    // print current buffer (bottom line stars)
    puts(screen);

    float  sample_rate = 48000;     // samples / seconds
    int    bytes_per_sample = 2;    // mpg321 outputs 16bit signed samples
    int    samples_per_frame = 2;   // stereo channels --> 2 samples per frame
    int    frames_per_packet = width - 1; // how wide we'll draw
    size_t samples_per_packet = frames_per_packet * samples_per_frame;  // 512 samples per packet
    size_t total_samples = fsize / bytes_per_sample;                    // 16 bit, dual channel audio. Number of frames.
    int    total_packets = total_samples / samples_per_packet; 
    float  packet_time = frames_per_packet / sample_rate;  
    int    usec_packet_time = (int) (packet_time * 1e6) - width * width * 0.025f;
    float  playtime = total_packets * packet_time;

    printf("%f per packet and %d packets for a total time of %f", packet_time, total_packets, playtime);

    // allocate vars on stack 
    int16_t* frame = audiobuf;
    int16_t left; 
    int16_t right;
    float relh;
    int absh;
    int row, col;

    // find min / max pcm sample for scaling
    int min_sample = 0xffffff;
    int max_sample = 0;
    for (size_t i = 0; i < total_samples; i++) {
        if (audiobuf[i] < min_sample) min_sample = audiobuf[i];
        if (audiobuf[i] > max_sample) max_sample = audiobuf[i];
    }
    printf("Min sample: %d, max sample: %d", min_sample, max_sample);

    // play the audio on speakers
    sprintf(command, "mpg321 %s &", argv[1]);
    system(command);
    
    // draw the audio in termianl 
    for (size_t i = 0; i < total_packets; i++) {
        memset(screen, ' ', width * height);
        for (int j = 0; j < frames_per_packet; j++) {
            left = frame[0];
            right = frame[1];
            frame += 2; 

            // draw left val
            relh = (float) ((int) left - min_sample) / max_sample - 1.0f;
            absh = height * relh * 0.95f;
            row = height - absh - 1; 
            col = j; 
            if (row > height) {
                row = height - 1;
            }
            if (row >= 0 && row < height && col >= 0 && col < width)
                for (int r = height - 1; r >= row; r--)
                    screen[r * width + col] = '*';
        }     
        puts(screen);

        // sleep depends on sample rate and terminal size
        usleep(usec_packet_time);
    }

    fclose(fp);
    free(audiobuf);
    free(screen);
}

