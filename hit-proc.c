/*
Requirements:

* Hits must be started a configurable time/number of samples before the signal passes above a configurable threshold.
* Hits last until the signal has been below another configurable threshold for at least some configurable time/number of samples or, until some configurable maximum time/number of samples has been reached.
* The software must provide the start time, channel, peak amplitude and integrated signal for a hit as well as whether it was cut off because of the maximum hit length or, because it fell below the threshold.

*/
#include <signal.h>
#include <iostream>
#include <fstream>
#include <inttypes.h>
#include <libgen.h>
#include <math.h>
#include <iomanip>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;
#define MAX_HITS 100
#define CHANNELS 2
#define SAMPLERATE 2000000.0
#define MAX_SAMP_R 4096
#define VOLTS_PER_BIT 0.0012

struct hit_t {
    int channel;
    int max;
    long start;
    long end;
    double integral;
} hits [MAX_HITS];

void print_hit(hit_t hit);
int process(std::istream& in, int chan0_zero, int chan1_zero, int start_thresh, int end_thresh, int samples_to_read, int pre_samp);
static double triangleIntegral(int a, int b);

// Used by sig_handler to tell us when to shutdown
static int bCont = 1;

void sig_handler (int sig) {
  // break out of reading loop
  bCont = 0;
  return;
}

void usage (char* arg0) 
{
    fprintf(stderr, "\nUsage: %s [flags]\n", basename(arg0));

    fprintf(stderr, "\n"
          "  -0 channel 0 offset\n"
          "  -1 channel 1 offset\n"
          "  -s start thresh\n"
          "  -p number of samples hit starts before threshold\n"
          "  -e end thresh\n"
          "  -i filename\n"
          "  -b samples to read\n\n"
         );
    exit(EXIT_FAILURE);
}

int main (int argc, char **argv) 
{
    int ch = -1;
    int chan0_zero = 0;
    int chan1_zero = 0;
    int start_thresh = 1000;
    int end_thresh = 500;
    int pre_samp = 0;
    int samples_to_read = 1024;
    int num_hits = 0;
    string filename = "-";

    // Process command line flags
    while (-1 != (ch = getopt(argc, argv, "p:i:0:1:s:e:b:"))) 
    {
    switch (ch) 
    {
        case 'p':
            pre_samp = strtod(optarg, NULL);
            break;
        case 'i':
            filename = optarg;
            break;
        case '0':
            chan0_zero = strtod(optarg, NULL);
            break;
        case '1':
            chan1_zero = strtod(optarg, NULL);
            break;
        case 'b':
            samples_to_read = strtod(optarg, NULL);
            if (samples_to_read %2 != 0) 
            {
                fprintf(stderr, "\nsamples_to_read must be divisible by 2\n");
                usage(argv[0]);
            }
            break;
        case 's':
            start_thresh = strtod(optarg, NULL);
            if (start_thresh == 0) 
            {
                fprintf(stderr, "\nstart threshold must be more than 0\n");
                usage(argv[0]);
            }
            break;
        case 'e':
            end_thresh = strtod(optarg, NULL);
            if (end_thresh == 0) 
            {
                fprintf(stderr, "\nend threshold must be more than 0\n");
                usage(argv[0]);
            }
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (argc - optind != 0) 
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

      // Install signal handler to catch ctrl-C
      if (SIG_ERR == signal(SIGINT, sig_handler)) {
        perror("Warn: signal handler not installed %d\n");
      }

    if(filename != "-")
    {
        ifstream file (filename.c_str(), ios::in|ios::binary|ios::ate);
        if (file.is_open())
        {
            file.seekg (0, ios::beg);
            num_hits = process(file, chan0_zero, chan1_zero, start_thresh, end_thresh, samples_to_read, pre_samp);
        }
        else
        {
            cout << "Unable to open file\n";
            return 0;
        }
    }
    else
    {
        // No input file has been passed in the command line.
        // Read the data from stdin (std::cin).
        cout << "reading from stdin\n";
        num_hits = process(std::cin, chan0_zero, chan1_zero, start_thresh, end_thresh, samples_to_read, pre_samp);
    }

    cout << "found " << num_hits << " hits:\n";
    for(int i=0; i< num_hits; i++)
        print_hit(hits[i]);

    return 0;
}

int process(std::istream& in, int chan0_zero, int chan1_zero, int start_thresh, int end_thresh, int samples_to_read, int pre_samp)
{
    long sample_number = 0;
    uint16_t array[samples_to_read];
    int hit_max[CHANNELS] = {0, 0};
    int last_sample[CHANNELS] = {0, 0};
    int hit_start[CHANNELS] = {0, 0};
    int hit_started[CHANNELS] = {0, 0};
    double hit_integral[CHANNELS] = {0, 0};
    int hit_number = 0;
    int zero[CHANNELS] = {chan0_zero, chan1_zero};
    int channel = 0;
    uint16_t sample = 0;

    cout << "starting\n";
    while(bCont && in.read((char*)array, samples_to_read * sizeof(uint16_t))) 
    {
        for(int i = 0; i < samples_to_read; i++)
        {
            channel = i % 2;
            sample = array[i];
            if(sample < zero[channel])
                sample = 0;
            else
                sample -= zero[channel];
            last_sample[channel] = sample;

            // if hit hasn't started
            if(! hit_started[channel])
            {
                if(sample > start_thresh)
                {
                    cout << "hit started at " << sample_number << " on channel " << channel << " val " << sample << "\n";
                    hit_started[channel] = 1;
                    hit_start[channel] = sample_number;
                    hit_max[channel] = 0;
                    hit_integral[channel] = 0;
                }
            }
            // else we're processing a hit
            else
            {
                // calculate integral
                hit_integral[channel] += triangleIntegral(last_sample[channel], sample);
                // do max
                if(sample > hit_max[channel])
                    hit_max[channel] = sample;
                // finish
                if(sample < end_thresh)
                {
                    cout << "hit ended at " << sample_number << " on channel " << channel << " val " << sample << "\n";
                    hit_started[channel] = 0;
                    // add struct
                    hits[hit_number].channel = channel;
                    hits[hit_number].max = hit_max[channel];
                    hits[hit_number].start = hit_start[channel] - pre_samp;
                    hits[hit_number].end = sample_number;
                    hits[hit_number].integral = hit_integral[channel];
                    hit_number ++;
                    if(hit_number > MAX_HITS)
                    {
                        cout << "ran out of hits\n";
                        break;
                    }
                }
            }
            //only increase every other sample
            if(channel == 1)
                sample_number ++;
        }
    }
    cout << "finished reading " << sample_number << " records\n";
    return hit_number;
}

static double triangleIntegral(int a, int b)
{
	return fabs((double)(a)+0.5*(double)(b-a));
}

void print_hit(hit_t hit)
{
    cout << "chan   " << hit.channel << "\n";
    cout << "start  " << std::setprecision(6) << hit.start/ SAMPLERATE << "(s)\n";
    cout << "end    " << std::setprecision(6) << hit.end / SAMPLERATE << "(s)\n";
    cout << "max    " << std::setprecision(6) << hit.max * VOLTS_PER_BIT << "(v)\n";
    cout << "len    " << std::setprecision(6) << 1000000 * (hit.end / SAMPLERATE- hit.start / SAMPLERATE)<< "(us)\n";
    cout << "integ  " << std::setprecision(6) << hit.integral * (VOLTS_PER_BIT/2) << "(uVs)\n"; // 2 instead of 2e6 so we get the result in uVs not Vs.

}
