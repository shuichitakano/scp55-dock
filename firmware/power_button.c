#include "power_button.h"
void scp55_power_button_init(scp55_power_button_t *s)
{
    *s=(scp55_power_button_t){.on=true};
}
void scp55_power_button_off(scp55_power_button_t *s)
{
    s->on=false;
    /* A held button must be released before another action. */
    s->consumed=s->raw || s->stable;
}
bool scp55_power_button_update(scp55_power_button_t *s, bool pressed, uint64_t now_ms)
{
    if(pressed!=s->raw) {s->raw=pressed;s->changed_ms=now_ms;}
    if(s->stable!=s->raw && now_ms-s->changed_ms>=30) {
        s->stable=s->raw;
        if(!s->stable) s->consumed=false;
        else {
            s->pressed_ms=now_ms;
            if(!s->on && !s->consumed) {s->on=true;s->consumed=true;}
        }
    }
    if(s->on && s->stable && s->raw && !s->consumed && now_ms-s->pressed_ms>=2000) {
        s->on=false;s->consumed=true;
    }
    return s->on;
}
