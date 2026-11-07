#include "midi_merge.h"
#include <assert.h>
#include <string.h>
static void feed(midi_merge_t *m,unsigned src,const uint8_t *p,unsigned n)
{assert(!midi_merge_push(m,src,p,n));}
static unsigned drain(midi_merge_t *m,uint8_t *p)
{unsigned n=0;while(n<100 && midi_merge_next(m,100,p+n))n++;return n;}
int main(void)
{
    midi_merge_t m;uint8_t out[100];midi_merge_init(&m);
    const uint8_t din[]={0x90,60},usb[]={0x91,64,90},end[]={80,62,70};
    feed(&m,0,din,2);feed(&m,1,usb,3);
    assert(drain(&m,out)==3 && !memcmp(out,usb,3));
    feed(&m,0,end,3);
    const uint8_t expected[]={0x90,60,80,62,70};
    assert(drain(&m,out)==5 && !memcmp(out,expected,5));
    /* System common cancels output status; DIN running status survives USB. */
    const uint8_t common[]={0xf2,1,2};feed(&m,1,common,3);
    assert(drain(&m,out)==3 && !memcmp(out,common,3));
    const uint8_t running[]={63,60};feed(&m,0,running,2);
    assert(drain(&m,out)==3 && out[0]==0x90 && out[1]==63);
    midi_merge_init(&m);
    const uint8_t sysex[]={0xf0,0x7e};feed(&m,0,sysex,2);
    assert(drain(&m,out)==2 && !memcmp(out,sysex,2));
    feed(&m,1,usb,3);assert(!midi_merge_next(&m,200,&out[0]));
    const uint8_t finish[]={0xf8,0x7f,0xf7};feed(&m,0,finish,3);
    assert(drain(&m,out)==6 && !memcmp(out,finish,3) && !memcmp(out+3,usb,3));
    midi_merge_init(&m);feed(&m,0,sysex,2);assert(drain(&m,out)==2);
    feed(&m,1,usb,3);
    assert(midi_merge_next(&m,500101,out) && out[0]==0xf7);
    assert(m.sysex_aborts==1);assert(drain(&m,out)==3 && !memcmp(out,usb,3));
    midi_merge_init(&m);feed(&m,0,sysex,2);assert(drain(&m,out)==2);
    midi_merge_reset_input(&m,0);assert(midi_merge_next(&m,200,out) && out[0]==0xf7);
    /* Real-time can appear inside a split message without changing its status. */
    midi_merge_init(&m);
    const uint8_t split[]={0x90,60,0xf8,99};feed(&m,0,split,sizeof split);
    assert(drain(&m,out)==4 && out[0]==0xf8 && out[1]==0x90 && out[2]==60 && out[3]==99);
    /* A new status aborts SysEx cleanly rather than leaking into its payload. */
    midi_merge_init(&m);feed(&m,0,sysex,2);assert(drain(&m,out)==2);
    feed(&m,0,usb,3);
    assert(drain(&m,out)==4 && out[0]==0xf7 && !memcmp(out+1,usb,3));
    /* System-common messages on one source cancel that source's running status. */
    midi_merge_init(&m);feed(&m,0,usb,3);assert(drain(&m,out)==3);
    feed(&m,0,common,3);assert(drain(&m,out)==3);
    feed(&m,0,running,2);assert(drain(&m,out)==0);
    uint8_t fill[MIDI_MERGE_CAPACITY]={0};feed(&m,1,fill,sizeof fill);
    assert(midi_merge_push(&m,1,usb,3)<0 && m.input[1].count==sizeof fill);
    return 0;
}
