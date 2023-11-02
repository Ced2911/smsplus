// #define SLAVE 1
// #undef BIGGEST_ALIGNMENT
// #define BIGGEST_ALIGNMENT 32

#define MAP 1

#ifndef _SHARED_H_
#define _SHARED_H_

typedef unsigned long int dword;
typedef unsigned short int word;
typedef unsigned char byte;

// #include "saturn/syscall.h"    /* for NULL, malloc(), etc. */
// #include "cz80/cz80.h"
#include "sms.h"
#include "vdp.h"
#include "render.h"
#include "system.h"
// #include "sega_sys.h"

#define SAMPLE 7680L

// #define CLEAR_LINE		0		/* clear (a fired, held or pulsed) line */
// #define ASSERT_LINE     1       /* assert an interrupt immediately */
// extern unsigned char *cpu_readmap[8];
// extern unsigned char *cpu_writemap[8];

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef unsigned long int uint32;

typedef signed char int8;
typedef signed short int int16;
typedef signed long int int32;

byte update_input(void);
byte update_input2(void);

extern void z80_cause_NMI(void);
extern void z80_lower_IRQ(void);
extern void z80_raise_IRQ(uint8 vector);
extern int z80_get_cycles_elapsed(void);
extern int z80_emulate(int cycles);
extern void z80_map_fetch(uint16 start, uint16 end, uint8 *memory);
extern void z80_map_read(uint16 start, uint16 end, uint8 *memory);
extern void z80_map_write(uint16 start, uint16 end, uint8 *memory);
extern void z80_add_write(uint16 start, uint16 end, int method, void *data);
extern void z80_init_memmap(void);
extern void z80_set_in(uint8 (*handler)(uint16 port));
extern void z80_set_out(void (*handler)(uint16 port, uint8 value));
extern void z80_end_memmap(void);
extern void z80_reset(void);
extern void z80_init(void);

#endif /* _SHARED_H_ */
