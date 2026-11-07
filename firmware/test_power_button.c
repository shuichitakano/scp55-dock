#include "power_button.h"
#include <assert.h>
int main(void) {
 scp55_power_button_t s;scp55_power_button_init(&s);
 assert(s.on);
 assert(scp55_power_button_update(&s,true,10));
 assert(scp55_power_button_update(&s,false,20)); /* bounce */
 assert(scp55_power_button_update(&s,true,100));
 assert(scp55_power_button_update(&s,true,130));
 assert(scp55_power_button_update(&s,true,2129));
 assert(!scp55_power_button_update(&s,true,2130));
 assert(!scp55_power_button_update(&s,true,6000)); /* no wake while held */
 assert(!scp55_power_button_update(&s,false,6010));
 assert(!scp55_power_button_update(&s,false,6040));
 assert(!scp55_power_button_update(&s,true,6100));
 assert(scp55_power_button_update(&s,true,6130)); /* short press wakes */
 assert(scp55_power_button_update(&s,true,10000)); /* wake press cannot turn off */
 assert(scp55_power_button_update(&s,false,10010));
 assert(scp55_power_button_update(&s,false,10040));
 assert(scp55_power_button_update(&s,true,10100));
 assert(scp55_power_button_update(&s,true,10130));
 assert(scp55_power_button_update(&s,false,10200));
 assert(scp55_power_button_update(&s,false,10230)); /* short press on does nothing */
 scp55_power_button_off(&s);assert(!s.on);
 assert(!scp55_power_button_update(&s,true,11000));
 assert(scp55_power_button_update(&s,true,11030));
 return 0;
}
