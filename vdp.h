
#ifndef _VDP_H_
#define _VDP_H_

/* Display timing (NTSC) */
#define MASTER_CLOCK (3579545)
#define LINES_PER_FRAME (262)
#define FRAMES_PER_SECOND (60)
#define CYCLES_PER_LINE_50 ((MASTER_CLOCK / 50) / LINES_PER_FRAME)
#define CYCLES_PER_LINE_60 ((MASTER_CLOCK / 60) / LINES_PER_FRAME)

/* VDP context */
typedef struct
{
    byte vram[0x4000];
    byte cram[0x40];
    byte reg[0x10];
    byte status;
    byte latch;
    byte pending;
    byte buffer;
    byte code;
    word addr;
    int ntab;
    int satb;
    int line;
    int left;

    byte scroll_x;
    byte scroll_y;
    //    byte limit;
} t_vdp;

/* Global data */
extern t_vdp vdp;

/* Function prototypes */
void vdp_init(void);
void vdp_reset(void);
void vdp_ctrl_w(int data);
int vdp_ctrl_r(void);
byte vdp_vcounter_r(void);
byte vdp_hcounter_r(void);
void vdp_data_w(int data);
int vdp_data_r(void);
void vdp_run(void);
// void vdp_run(t_vdp *vdp);

void make_name_lut(void);
void make_bp_lut(void);
void make_cram_lut(void);

#endif /* _VDP_H_ */
