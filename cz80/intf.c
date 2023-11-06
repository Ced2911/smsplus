#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../shared.h"
#include "cz80m.h"
#define UNUSED(x) (void)(x)

//====================================================
#define MAP_READ 1
#define MAP_WRITE 2
#define MAP_FETCHOP 4
#define MAP_FETCHARG 8
#define MAP_FETCH (MAP_FETCHOP | MAP_FETCHARG)
#define MAP_ROM (MAP_READ | MAP_FETCH)
#define MAP_RAM (MAP_ROM | MAP_WRITE)

// #define CZET_IRQSTATUS_NONE 0 //CZ80_IRQSTATUS_NONE
// #define CZET_IRQSTATUS_AUTO 2//CZ80_IRQSTATUS_AUTO
// #define CZET_IRQSTATUS_ACK  1//CZ80_IRQSTATUS_ACK
#define CZET_IRQSTATUS_NONE CZ80_IRQSTATUS_NONE
#define CZET_IRQSTATUS_AUTO CZ80_IRQSTATUS_AUTO
#define CZET_IRQSTATUS_ACK CZ80_IRQSTATUS_ACK
#define CZET_IRQSTATUS_HOLD 4
#define CZetRaiseIrq(n) CZetSetIRQLine(n, CZET_IRQSTATUS_AUTO)
#define CZetLowerIrq() CZetSetIRQLine(0, CZET_IRQSTATUS_NONE)

int CZetGetPC(int n);
int CZetInit2(int nCount, UINT8 *addr);
void CZetExit2();
void CZetNewFrame();
int CZetOpen(int nCPU);
void CZetClose();
int CZetMemCallback(int nStart, int nEnd, int nMode);
int CZetMemEnd();
void CZetMapMemory(unsigned char *Mem, int nStart, int nEnd, int nFlags);
void CZetMapMemory2(unsigned char *Mem, unsigned char *Mem02, int nStart, int nEnd, int nFlags);
int CZetMapArea(int nStart, int nEnd, int nMode, unsigned char *Mem);
int CZetMapArea2(int nStart, int nEnd, int nMode, unsigned char *Mem01, unsigned char *Mem02);
void CZetReset();
int CZetPc(int n);
int CZetBc(int n);
int CZetHL(int n);
int CZetScan(int nAction);
int CZetRun(int nCycles);
void CZetRunEnd();
void CZetSetIRQLine(const int line, const int status);
int CZetNmi();
int CZetIdle(int nCycles);
int CZetSegmentCycles();
int CZetTotalCycles();

#define MAX_CPUS 4

void CZetSetReadHandler(unsigned char(__fastcall *pHandler)(unsigned short));
void CZetSetWriteHandler(void(__fastcall *pHandler)(unsigned short, unsigned char));
void CZetSetInHandler(unsigned char(__fastcall *pHandler)(unsigned short));
void CZetSetOutHandler(void(__fastcall *pHandler)(unsigned short, unsigned char));

//====================================================
unsigned char CZetDummyReadHandler(unsigned short a)
{
    UNUSED(a);
    return 0;
}
void CZetDummyWriteHandler(unsigned short a, unsigned char b)
{
    UNUSED(a);
    UNUSED(b);
}
unsigned char CZetDummyInHandler(unsigned short a)
{
    UNUSED(a);
    return 0;
}
void CZetDummyOutHandler(unsigned short a, unsigned char b)
{
    UNUSED(a);
    UNUSED(b);
}

static cz80_struc CZ80Context;
static cz80_struc *CZetCPUContext = &CZ80Context;

int CZetNmi()
{
    int nCycles = Cz80_Set_NMI(CZetCPUContext);
    CZetCPUContext->nCyclesTotal += nCycles;

    return nCycles;
}

void CZetSetIRQLine(const int line, const int status)
{
    CZetCPUContext->nInterruptLatch = line | status;
}

int CZetRun(int nCycles)
{
    if (nCycles <= 0)
    {
        return 0;
    }

    CZetCPUContext->nCyclesTotal += nCycles;
    CZetCPUContext->nCyclesSegment = nCycles;
    CZetCPUContext->nCyclesLeft = nCycles;

    nCycles = Cz80_Exec(CZetCPUContext);

    CZetCPUContext->nCyclesTotal -= CZetCPUContext->nCyclesLeft;
    CZetCPUContext->nCyclesLeft = 0;
    CZetCPUContext->nCyclesSegment = 0;

    return nCycles;
}
void CZetReset()
{
    Cz80_Reset(CZetCPUContext);
}

int CZetInit()
{
    Cz80_InitFlags();

    Cz80_Init(CZetCPUContext);
    CZetCPUContext->nInterruptLatch = -1;

    CZetCPUContext->Read_Byte = CZetDummyReadHandler;
    CZetCPUContext->Write_Byte = CZetDummyWriteHandler;
    CZetCPUContext->Read_Word = CZetDummyReadHandler;
    CZetCPUContext->Write_Word = CZetDummyWriteHandler;
    CZetCPUContext->IN_Port = CZetDummyInHandler;
    CZetCPUContext->OUT_Port = CZetDummyOutHandler;

    return 0;
}

int CZetMapArea(int nStart, int nEnd, int nMode, unsigned char *Mem)
{
    unsigned int s = nStart >> CZ80_FETCH_SFT;
    unsigned int e = (nEnd + CZ80_FETCH_BANK - 1) >> CZ80_FETCH_SFT;
    static void (*const nMode_table[3])(void) = {&&Read, &&Write, &&Fetch};
#define DISPATCH() goto *nMode_table[nMode]

    for (unsigned int i = s; i < e; i++)
    {
        DISPATCH();
    Read:
        CZetCPUContext->Read[i] = Mem - nStart;
        goto End;
    Write:
        CZetCPUContext->Write[i] = Mem - nStart;
        goto End;
    Fetch:
        CZetCPUContext->Fetch[i] = Mem - nStart;
        CZetCPUContext->FetchData[i] = Mem - nStart;
    End:;
    }
    return 0;
}
void CZetMapMemory(unsigned char *Mem, int nStart, int nEnd, int nFlags)
{
    //	UINT8 cStart = (nStart >> 8);
    int s = nStart >> CZ80_FETCH_SFT;
    //	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;
    int e = (nEnd + CZ80_FETCH_BANK - 1) >> CZ80_FETCH_SFT;

    for (int i = s; i < e; i++)
    {
        if (nFlags & (1 << 0))
            CZetCPUContext->Read[i] = Mem - nStart; // READ
        if (nFlags & (1 << 1))
            CZetCPUContext->Write[i] = Mem - nStart; // WRITE
        if (nFlags & (1 << 2))
            CZetCPUContext->Fetch[i] = Mem - nStart; // OP
        if (nFlags & (1 << 3))
            CZetCPUContext->FetchData[i] = Mem - nStart; // ARG
    }
}

void CZetSetInHandler(unsigned char (*pHandler)(unsigned short))
{
    CZetCPUContext->IN_Port = pHandler;
}

void CZetSetOutHandler(void (*pHandler)(unsigned short, unsigned char))
{
    CZetCPUContext->OUT_Port = pHandler;
}
void CZetSetWriteHandler(void (*pHandler)(uint16_t, uint8_t))
{
    CZetCPUContext->Write_Byte = pHandler;
}

void CZetSetWriteHandler2(unsigned short nStart, unsigned short nEnd, write_func pHandler)
{
    UINT8 cStart = (nStart >> 8);

    for (UINT32 i = cStart; i <= (nEnd >> 8); i++)
    {
        CZetCPUContext->wf[i] = pHandler;
    }
}
void CZetSetSP(unsigned short data)
{
    CZetCPUContext->SP.W = data;
}
int CZetTotalCycles()
{
    return CZetCPUContext->nCyclesTotal - CZetCPUContext->nCyclesLeft;
}
//====================================================

void z80_raise_IRQ(uint8_t vector)
{
    UNUSED(vector);
    CZetRaiseIrq(0);
}

void z80_cause_NMI(void)
{
    CZetNmi();
}

int z80_emulate(int cycles)
{
    return CZetRun(cycles);
}

int z80_get_cycles_elapsed()
{
    return CZetTotalCycles();
}
void z80_lower_IRQ()
{
    CZetLowerIrq();
}

//====================================================

extern t_sms sms;
extern t_cart cart;
extern unsigned int sound;
extern unsigned dummy_write[0x100];

static __inline__ void cpu_writemem8(uint16_t address, uint8_t data)
{
    sms.ram[address & 0x1FFF] = data;
    // data & cart.pages, and set cart.pages to one less than you are
    uint32 offset = (data % cart.pages) << 14; // VBT � corriger
                                               // vbt 15/05/2008 : exophase :	 data & cart.pages, and set cart.pages to one less than you are
    sms.fcr[address & 3] = data;

    switch (address & 3)
    {
    case 0:

        if (data & 8)
        {
            offset = (data & 0x4) ? 0x4000 : 0x0000;

            CZetMapMemory((unsigned char *)(sms.sram + offset), 0x8000, 0xBFFF, MAP_RAM);
        }
        else
        {
            offset = ((sms.fcr[3] % cart.pages) << 14);
            // vbt 15/05/2008 : exophase :	 data & cart.pages, and set cart.pages to one less than you are
            CZetMapMemory((unsigned char *)(cart.rom + offset), 0x8000, 0xBFFF, MAP_ROM);
            CZetMapMemory((unsigned char *)(dummy_write), 0x8000, 0xBFFF, MAP_WRITE);
        }
        break;
    case 1:
        CZetMapMemory((unsigned char *)(cart.rom + offset), 0x0000, 0x3FFF, MAP_ROM);

        break;

    case 2:
        CZetMapMemory((unsigned char *)(cart.rom + offset), 0x4000, 0x7FFF, MAP_ROM);
        break;

    case 3:

        if (!(sms.fcr[0] & 0x08))
        {
            CZetMapMemory((unsigned char *)(cart.rom + offset), 0x8000, 0xBFFF, MAP_ROM);
        }
        break;
    }
    return;
}
//---------------------------------------------------------------------------
static void cz80_z80_writeport16(unsigned short PortNo, unsigned char data)
{
    switch (PortNo & 0xFF)
    {
    case 0x01: /* GG SIO */
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06: /* GG STEREO */
    case 0x7E: /* SN76489 PSG */
    case 0x7F:
#ifdef SOUND
        if (sound)
#ifndef CED
            //				SN76496Write(0,data);
            PSG_Write(data);
#else
            sn76496_w(data);
#endif

#endif
        break;

    case 0xBE: /* VDP DATA */
        vdp_data_w(data);
        break;

    case 0xBD: /* VDP CTRL */
    case 0xBF:
        //			printf ("write vdp_ctrl_w");
        vdp_ctrl_w(data);
        break;

    case 0xF0: /* YM2413 */
    case 0xF1:
        //            ym2413_write(0, PortNo & 1, data);
        break;

    case 0xF2: /* YM2413 DETECT */
               //            sms.port_F2 = (data & 1);
        break;

    case 0x3F: /* TERRITORY CTRL. */
        sms.port_3F = ((data & 0x80) | (data & 0x20) << 1) & 0xC0;
        if (sms.country == TYPE_DOMESTIC)
            sms.port_3F ^= 0xC0;
        break;
    }
}
//---------------------------------------------------------------------------
static unsigned char cz80_z80_readport16(unsigned short PortNo)
{
    switch (PortNo & 0xFF)
    {
    case 0x7E: /* V COUNTER */
        return (vdp_vcounter_r());
        break;

    case 0x7F: /* H COUNTER */
               //	printf("vdp_hcounter_r\n");
        return (vdp_hcounter_r());
        break;

    case 0x00: /* INPUT #2 */
        return 0xff;
        //            temp = 0xFF;
        //			return (update_system());
        //            if(input.system & INPUT_START) temp &= ~0x80;
        //            if(sms.country == TYPE_DOMESTIC) temp &= ~0x40;
        //            return (temp);

    case 0xC0: /* INPUT #0 */
    case 0xDC:
        return (update_input());
        //			return 0xff;
    case 0xC1: /* INPUT #1 */
    case 0xDD:
        //			return 0xff;
        return update_input2();

    case 0xBE: /* VDP DATA */
               //			printf ("read port vdp_data_r\n");
        return (vdp_data_r());

    case 0xBD:
    case 0xBF: /* VDP CTRL */
               //			printf ("read port vdp_ctrl_r\n");
        return (vdp_ctrl_r());

    case 0xF2: /* YM2413 DETECT */
               //            return (sms.port_F2);
        break;
    }
    //    return (0xFF);
    return (0);
}

void z80_init(void)
{

    CZetInit();
    CZetReset();

    /* Bank #0 */
    CZetMapMemory((unsigned char *)cart.rom, 0x0000, 0x3FFF, MAP_ROM);

    /* Bank #1 */
    CZetMapMemory((unsigned char *)cart.rom + 0x4000, 0x4000, 0x7FFF, MAP_ROM);

    /* Bank #2 */
    CZetMapMemory((unsigned char *)cart.rom + 0x8000, 0x8000, 0xBFFF, MAP_ROM);

    /* RAM */
    CZetMapMemory((unsigned char *)cart.rom, 0xC000, 0xDFFF, MAP_RAM);

    /* RAM (mirror) */
    CZetMapMemory((unsigned char *)cart.rom, 0xE000, 0xFFFF, MAP_ROM);
    CZetSetWriteHandler(cpu_writemem8);
    CZetSetInHandler(cz80_z80_readport16);
    CZetSetOutHandler(cz80_z80_writeport16);

    CZetReset();
}

void sms_reset(void)
{
    z80_init();
    /* Clear SMS context */
    memset(dummy_write, 0, 0x100);
    memset(sms.ram, 0, 0x2000);
    memset(sms.sram, 0, 0x8000);
    sms.port_3F = sms.port_F2 = sms.irq = 0x00;
    //    sms.psg_mask = 0xFF;

    CZetMapArea(0x0000, 0x3FFF, 0, cart.rom);
    CZetMapMemory((unsigned char *)cart.rom, 0x0000, 0x3FFF, MAP_READ);
    CZetMapMemory((unsigned char *)&cart.rom[0x4000], 0x4000, 0x7FFF, MAP_READ);
    CZetMapMemory((unsigned char *)&cart.rom[0x8000], 0x8000, 0xBFFF, MAP_READ);
    CZetMapMemory((unsigned char *)dummy_write, 0x0000, 0xBFFF, MAP_WRITE);
    CZetMapMemory((unsigned char *)sms.ram, 0xC000, 0xDFFF, MAP_RAM);
    CZetMapMemory((unsigned char *)sms.ram, 0xE000, 0xFFFF, MAP_READ);

    //	CZetSetWriteHandler(cpu_writemem8);
    CZetSetWriteHandler2(0xFFFC, 0xFFFF, cpu_writemem8);
    CZetSetSP(0xdff0);

    sms.fcr[0] = 0x00;
    sms.fcr[1] = 0x00;
    sms.fcr[2] = 0x01;
    sms.fcr[3] = 0x00;
}
