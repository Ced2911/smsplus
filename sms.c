#include <yaul.h>
#include <stdint.h>
#include "shared.h"
#include "snd/scsp.h"
#include "snd/sn76496ced.h"

// #define DEBUG 1
unsigned dummy_write[0x100];
fix16_t ls_tbl[3 * 256];
t_sms sms;
int scroll_x = 0, scroll_y = 0;
unsigned int first = 1;

/* Run the virtual console emulation for one frame */
void sms_frame(int skip_render)
{
   if (sms.paused && first == 2)
   {
      z80_cause_NMI();
      first = 0;
   }

   for (vdp.line = 0; vdp.line < 262; vdp.line++)
   {
      z80_emulate(228);

      if (vdp.line < 0x10 && (vdp.reg[0] & 0x40))
         ls_tbl[vdp.line] = (-47 << 16);
      else if (vdp.line < 0xC0)
         ls_tbl[vdp.line] = scroll_x - (47 << 16);

      vdp_run();
   }
}

void sms_init(void)
{
   z80_init();
   sms_reset();
}

int sms_irq_callback(int param)
{
   return (0);
}
