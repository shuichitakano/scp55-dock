#include "audio_resample.h"
#include <assert.h>
int main(void)
{
    scp55_resampler_t s;
    scp55_audio_ring_t r;
    scp55_audio_frame_t f;
    scp55_resampler_init(&s);scp55_audio_ring_init(&r);
    /* Constant DC must produce silence even when far from zero. */
    for (unsigned i=0;i<100;++i) {
        assert(scp55_resampler_push(&s,&r,1200)==0);
        for (unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left==0&&f.right==0);}
    }
    assert(!scp55_resampler_push(&s,&r,1400));
    int16_t previous=0;
    for (unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left>previous&&f.left==f.right);previous=f.left;}
    assert(!scp55_resampler_push(&s,&r,INT16_MIN));
    for (unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left>=-16384&&f.left<=16384);}
    assert(s.clipped==1);
    for (unsigned i=0;i<SCP55_AUDIO_RING_FRAMES-2;++i) assert(!scp55_audio_ring_push(&r,(scp55_audio_frame_t){0}));
    uint32_t before=scp55_audio_ring_count(&r);
    assert(scp55_resampler_push(&s,&r,0)<0);
    assert(scp55_audio_ring_count(&r)==before); /* No partial group on overflow. */
    scp55_stereo_resampler_t stereo;
    scp55_stereo_resampler_init(&stereo);scp55_audio_ring_init(&r);
    assert(!scp55_stereo_resampler_push(&stereo,&r,1000,-800));
    for(unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left==0&&f.right==0);}
    assert(!scp55_stereo_resampler_push(&stereo,&r,1200,-800));
    for(unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left>0&&f.right==0);}
    assert(!scp55_stereo_resampler_push(&stereo,&r,1200,-1000));
    for(unsigned j=0;j<4;++j) {assert(!scp55_audio_ring_pop(&r,&f));assert(f.left>0&&f.right<0);}
    assert(!scp55_stereo_resampler_push(&stereo,&r,INT16_MAX,-1000));
    assert(stereo.left.clipped==0 && stereo.right.clipped==0);
    while(scp55_audio_ring_count(&r)<SCP55_AUDIO_RING_FRAMES-2) assert(!scp55_audio_ring_push(&r,(scp55_audio_frame_t){0}));
    before=scp55_audio_ring_count(&r);
    int32_t dc=stereo.left.dc_q8;
    assert(scp55_stereo_resampler_push(&stereo,&r,0,0)<0);
    assert(scp55_audio_ring_count(&r)==before && stereo.left.dc_q8==dc);
    /* Higher capture rates preserve channel identity and exact output counts. */
    const unsigned rates[]={22050,44100};
    for(unsigned k=0;k<2;k++) {
        assert(!scp55_stereo_resampler_init_rate(&stereo,rates[k]));
        scp55_audio_ring_init(&r);
        assert(!scp55_stereo_resampler_push(&stereo,&r,0,0));
        assert(scp55_audio_ring_count(&r)==44100/rates[k]);
        while(!scp55_audio_ring_pop(&r,&f)) {}
        assert(!scp55_stereo_resampler_push(&stereo,&r,500,-500));
        assert(scp55_audio_ring_count(&r)==44100/rates[k]);
        while(!scp55_audio_ring_pop(&r,&f)) assert(f.left==-f.right);
    }
    /* Native 44.1 kHz must not smooth rapid changes or add FIR latency. */
    assert(!scp55_stereo_resampler_init_rate(&stereo,44100));
    scp55_audio_ring_init(&r);
    assert(!scp55_stereo_resampler_push(&stereo,&r,0,0));
    assert(!scp55_audio_ring_pop(&r,&f));
    for(unsigned i=0;i<1000;i++) {
        int16_t input=(i&1)?-1000:1000;
        assert(!scp55_stereo_resampler_push(&stereo,&r,input,0));
        assert(scp55_audio_ring_count(&r)==1);
        assert(!scp55_audio_ring_pop(&r,&f));
        assert(f.left==input && f.right==0);
    }
    /* Regression: measured loud-chord peaks around +/-9000 must not clip.
     * Exercise up to +/-30000, beyond the former half-scale ceiling. */
    for(int amplitude=9000;amplitude<=30000;amplitude+=7000) {
        assert(!scp55_stereo_resampler_init_rate(&stereo,44100));
        scp55_audio_ring_init(&r);
        assert(!scp55_stereo_resampler_push(&stereo,&r,0,0));
        assert(!scp55_audio_ring_pop(&r,&f));
        for(unsigned i=0;i<1000;i++) {
            int16_t input=(i&1)?-amplitude:amplitude;
            assert(!scp55_stereo_resampler_push(&stereo,&r,input,-input));
            assert(!scp55_audio_ring_pop(&r,&f));
            assert(f.left>input-20 && f.left<input+20);
            assert(f.left==-f.right);
        }
        assert(!stereo.left.clipped && !stereo.right.clipped);
    }
    /* DC removal can exceed int16 range on an extreme discontinuity. */
    assert(!scp55_stereo_resampler_init_rate(&stereo,44100));
    assert(!scp55_stereo_resampler_push(&stereo,&r,INT16_MIN,INT16_MAX));
    assert(!scp55_audio_ring_pop(&r,&f));
    assert(!scp55_stereo_resampler_push(&stereo,&r,INT16_MAX,INT16_MIN));
    assert(!scp55_audio_ring_pop(&r,&f));
    assert(f.left==INT16_MAX && f.right==INT16_MIN);
    assert(stereo.left.clipped==1 && stereo.right.clipped==1);
    assert(scp55_stereo_resampler_init_rate(&stereo,48000)<0);
    return 0;
}
