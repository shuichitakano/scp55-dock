#include "audio_resample.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static const double pi=3.14159265358979323846;
static void response(double frequency)
{
    scp55_stereo_resampler_t state;
    scp55_audio_ring_t ring;
    scp55_audio_ring_init(&ring);
    assert(!scp55_stereo_resampler_init_rate(&state,22050));
    double desired_re=0,desired_im=0,image_re=0,image_im=0;
    unsigned samples=0;
    for(unsigned n=0;n<24255;n++) {
        int16_t input=(int16_t)lround(400*sin(2*pi*frequency*n/22050));
        assert(!scp55_stereo_resampler_push(&state,&ring,input,0));
        for(unsigned j=0;j<2;j++) {
            scp55_audio_frame_t frame;
            assert(!scp55_audio_ring_pop(&ring,&frame));
            assert(frame.right==0); /* No crosstalk from the resampler. */
            if(n<2205)continue;
            double t=(2*n+j)/44100.0;
            desired_re+=frame.left*cos(2*pi*frequency*t);
            desired_im+=frame.left*sin(2*pi*frequency*t);
            image_re+=frame.left*cos(2*pi*(22050-frequency)*t);
            image_im+=frame.left*sin(2*pi*(22050-frequency)*t);
            samples++;
        }
    }
    double gain=2*hypot(desired_re,desired_im)/samples/400;
    double image=hypot(image_re,image_im)/hypot(desired_re,desired_im);
    printf("FIR %.0f Hz: gain %.6f, image %.1f dB\n",frequency,gain,20*log10(image));
    assert(gain>0.98 && gain<1.02);
    assert(image<0.004); /* Better than -48 dB at these probe frequencies. */
    assert(!state.left.clipped && !state.right.clipped);
}
int main(void) {response(1000);response(8000);return 0;}
