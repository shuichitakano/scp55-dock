#include "audio_resample.h"
void scp55_resampler_init(scp55_resampler_t *s) { *s = (scp55_resampler_t){0}; }
int scp55_resampler_push(scp55_resampler_t *s, scp55_audio_ring_t *r, int16_t sample)
{
    if (!s || !r) return -1;
    if (scp55_audio_ring_count(r) > SCP55_AUDIO_RING_FRAMES-4) return -1;
    if (!s->primed) { s->dc_q8 = (int32_t)sample*256; s->primed = true; }
    s->dc_q8 += ((int32_t)sample*256-s->dc_q8)/512;
    int32_t next = ((int32_t)sample-s->dc_q8/256)*8;
    if (next > 16384) { next=16384; ++s->clipped; }
    if (next < -16384) { next=-16384; ++s->clipped; }
    for (int j=1;j<=4;++j) {
        int16_t v=(int16_t)(s->previous+(next-s->previous)*j/4);
        scp55_audio_frame_t f={v,v};
        if (scp55_audio_ring_push(r,f)) return -1;
    }
    s->previous=next;
    return 0;
}

void scp55_stereo_resampler_init(scp55_stereo_resampler_t *s)
{
    (void)scp55_stereo_resampler_init_rate(s,11025);
}
int scp55_stereo_resampler_init_rate(scp55_stereo_resampler_t *s, unsigned rate)
{
    if(!s || (rate!=11025 && rate!=22050 && rate!=44100)) return -1;
    *s=(scp55_stereo_resampler_t){0};
    scp55_resampler_init(&s->left);scp55_resampler_init(&s->right);
    s->factor=44100/rate;s->dc_divisor=2048/s->factor;
    return 0;
}
static int32_t channel_next(scp55_resampler_t *s, int16_t sample, unsigned dc_divisor)
{
    if (!s->primed) {s->dc_q8=(int32_t)sample*256;s->primed=true;}
    s->dc_q8+=((int32_t)sample*256-s->dc_q8)/(int32_t)dc_divisor;
    int32_t next=((int32_t)sample-s->dc_q8/256);
    if(next>32767) {next=32767;++s->clipped;}
    if(next< -32768) {next=-32768;++s->clipped;}
    return next;
}
int scp55_stereo_resampler_push(scp55_stereo_resampler_t *s, scp55_audio_ring_t *r, int16_t left, int16_t right)
{
    if(!s || !r || (s->factor!=1 && s->factor!=2 && s->factor!=4) || scp55_audio_ring_count(r)>SCP55_AUDIO_RING_FRAMES-s->factor) return -1;
    int32_t l=channel_next(&s->left,left,s->dc_divisor), rnext=channel_next(&s->right,right,s->dc_divisor);
    if(s->factor==2) {
        /* 31-tap half-band FIR, Q15 even phase. Odd phase is a 7-input-frame
         * delay (the center tap is unity after interpolation gain).
         * Unity DC per phase; suppresses images without linear-interpolator
         * high-frequency droop. Approximately 0.34 ms group delay. */
        static const int32_t coefficients[16]={-112,193,-442,925,-1758,3223,-6363,20718,20718,-6363,3223,-1758,925,-442,193,-112};
        unsigned pos=s->history_pos;
        s->history_left[pos]=l;s->history_right[pos]=rnext;
        int64_t acc_l=0,acc_r=0;
        for(unsigned tap=0;tap<16;tap++) {
            unsigned index=(pos+16-tap)%16;
            acc_l+=(int64_t)coefficients[tap]*s->history_left[index];
            acc_r+=(int64_t)coefficients[tap]*s->history_right[index];
        }
        int32_t even_l=(int32_t)(acc_l/32768),even_r=(int32_t)(acc_r/32768);
        if(even_l>32767) {even_l=32767;++s->left.clipped;}
        if(even_l< -32768) {even_l=-32768;++s->left.clipped;}
        if(even_r>32767) {even_r=32767;++s->right.clipped;}
        if(even_r< -32768) {even_r=-32768;++s->right.clipped;}
        unsigned delay=(pos+16-7)%16;
        scp55_audio_frame_t even={(int16_t)even_l,(int16_t)even_r};
        scp55_audio_frame_t odd={(int16_t)s->history_left[delay],(int16_t)s->history_right[delay]};
        if(scp55_audio_ring_push(r,even) || scp55_audio_ring_push(r,odd)) return -1;
        s->history_pos=(pos+1)%16;
        s->left.previous=l;s->right.previous=rnext;
        return 0;
    }
    for(int j=1;j<=(int)s->factor;++j) {
        scp55_audio_frame_t f={
            (int16_t)(s->left.previous+(l-s->left.previous)*j/(int)s->factor),
            (int16_t)(s->right.previous+(rnext-s->right.previous)*j/(int)s->factor)};
        if(scp55_audio_ring_push(r,f)) return -1;
    }
    s->left.previous=l;s->right.previous=rnext;
    return 0;
}
