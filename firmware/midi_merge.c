#include "midi_merge.h"
#include <string.h>
void midi_merge_init(midi_merge_t *m) {*m=(midi_merge_t){.sysex_owner=-1};}
unsigned midi_merge_space(const midi_merge_t *m,unsigned source)
{return source<2 ? MIDI_MERGE_CAPACITY-m->input[source].count : 0;}
int midi_merge_push(midi_merge_t *m,unsigned source,const uint8_t *bytes,unsigned count)
{
    if(source>=2 || !bytes || count>midi_merge_space(m,source)) return -1;
    midi_merge_input_t *s=&m->input[source];
    for(unsigned i=0;i<count;i++) s->bytes[s->head++%MIDI_MERGE_CAPACITY]=bytes[i];
    s->count+=count;return 0;
}
void midi_merge_reset_input(midi_merge_t *m,unsigned source)
{
    if(source>=2)return;
    m->input[source]=(midi_merge_input_t){0};
    if(m->sysex_owner==(int)source) {m->sysex_owner=-1;m->end_sysex=true;++m->sysex_aborts;}
}
static uint8_t pop(midi_merge_input_t *s)
{--s->count;return s->bytes[s->tail++%MIDI_MERGE_CAPACITY];}
static bool pending(midi_merge_t *m,uint8_t *out)
{
    if(m->pending_pos==m->pending_count)return false;
    *out=m->pending[m->pending_pos++];return true;
}
bool midi_merge_next(midi_merge_t *m,uint64_t now,uint8_t *out)
{
    if(pending(m,out))return true;
    if(m->end_sysex) {m->end_sysex=false;m->output_running=0;*out=0xf7;return true;}
    if(m->sysex_owner>=0) {
        midi_merge_input_t *s=&m->input[m->sysex_owner];
        if(!s->count) {
            if(now-m->sysex_activity_us<500000)return false;
            midi_merge_reset_input(m,(unsigned)m->sysex_owner);
            return midi_merge_next(m,now,out);
        }
        uint8_t b=s->bytes[s->tail%MIDI_MERGE_CAPACITY];
        if((b&0x80) && b<0xf8 && b!=0xf7) {
            /* An interrupted SysEx must end before a new status from either input. */
            m->sysex_owner=-1;++m->sysex_aborts;*out=0xf7;return true;
        }
        *out=pop(s);
        /* Real-time traffic cannot keep an abandoned SysEx locked forever. */
        if(b<0xf8) m->sysex_activity_us=now;
        if(b==0xf7) m->sysex_owner=-1;
        else if(b>=0xf8 && now-m->sysex_activity_us>=500000) {
            midi_merge_reset_input(m,(unsigned)m->sysex_owner);
        }
        return true;
    }
    for(unsigned attempt=0;attempt<2;attempt++) {
        unsigned source=m->turn;m->turn^=1;
        midi_merge_input_t *s=&m->input[source];
        for(unsigned step=0;step<8 && s->count;step++) {
            uint8_t b=pop(s);
            if(b>=0xf8) {*out=b;return true;}
            if(b&0x80) {
                s->used=0;
                if(b<0xf0) {
                    s->running=b;s->message[0]=b;s->used=1;
                    s->length=((b&0xe0)==0xc0)?2:3;
                } else {
                    s->running=0;
                    if(b==0xf0) {
                        m->sysex_owner=(int)source;m->sysex_activity_us=now;
                        m->output_running=0;*out=b;return true;
                    }
                    if(b==0xf1 || b==0xf2 || b==0xf3) {
                        s->message[0]=b;s->used=1;s->length=b==0xf2?3:2;
                    } else if(b==0xf6 || b==0xf7) {
                        m->output_running=0;*out=b;return true;
                    }
                }
                continue;
            }
            if(!s->used) {
                if(!s->running)continue; /* orphan data is not a message */
                s->message[0]=s->running;s->used=1;
                s->length=((s->running&0xe0)==0xc0)?2:3;
            }
            s->message[s->used++]=b;
            if(s->used==s->length) {
                uint8_t status=s->message[0];
                unsigned skip=status<0xf0 && status==m->output_running ? 1 : 0;
                m->pending_count=s->length-skip;m->pending_pos=0;
                memcpy(m->pending,s->message+skip,m->pending_count);
                m->output_running=status<0xf0?status:0;s->used=0;
                return pending(m,out);
            }
        }
    }
    return false;
}
