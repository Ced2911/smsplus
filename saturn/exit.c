/*****************************************************************************
 *      �f���f���\�t�g�t�@�C�����[�_
 *
 *      Copyright (c) 1995 CSK Research Institute Corp.
 *      Copyright (c) 1995 SEGA
 *
 * Library:FLD
 * Module :�I���֐�
 * File   :exit.c
 * Date   :1995-08-03
 * Version:1.10
 * Auther :T.K.
 *
 *****************************************************************************/

/*****************************************************************************
 *      �C���N���[�h�t�@�C��
 *****************************************************************************/
#ifdef	__GNUC__
	#include <machine.h>
#else
	#include <machine2.h>
#endif

#include "sega_sys.h"
#include "sega_cdc.h"
#include "sega_gfs.h"
#include "sega_per.h"

/*****************************************************************************
 *      �萔�}�N��
 *****************************************************************************/
/* �i��ԍ� */
#define FLT_READ        0       /* ��Ɨp */
#define FLT_DUMMY       23      /* ���[�g�f�B���N�g���ւ̈ړ��p */

/* �^�C���A�E�g�l */
#define TMOUT_HIRQREQ    2900000    /* HIRQREQ�̃f�t�H���g :  4s */
#define TMOUT_IPLTSK    15000000    /* IP�ǂݍ��݃^�X�N    : 21s */

/* �X�^�b�N�|�C���^ */
#define STACKPTR        0x06002000

/*****************************************************************************
 *      �����}�N��
 *****************************************************************************/
/* IP���[�h�����i�������A�^�j */
#define SYS_IPLGAMECD(dummy) \
            ((**(Sint32 (**)(Sint32))0x600029c)(dummy))

/* IP�`�F�b�N�������i�������A�^�j */
#define SYS_RUNGAMECD(dummy) \
            ((**(Sint32 (**)(Sint32))0x6000288)(dummy))

/*****************************************************************************
 *      �֐��̐錾
 *****************************************************************************/
static void restart(void);
static void execRestart(void);
static void initSaturn(void);
static Sint32 resetCd(void);
static Sint32 waitHirq(Sint32 flag);
static void chgDirRootDummy(void);
static void execIpChkRun(void);
static Sint32 setStackptr(Sint32 addr);

/*****************************************************************************
 *      �֐��̒�`
 *****************************************************************************/

/*
 * �I���֐�
 * [����]
 *      code : �@�\�R�[�h
 *           0      : �ċN���iIP�`�F�b�N�������̎��s�j
 *           1      : �}���`�v���[���̋N��
 *           ���̒l : �������[�v
 *           ���̑� : 0�Ɠ����i�f�t�H���g�j
 */
void exit(Sint32 code)
{
    switch(code) {
    case 1:     /* �}���`�v���[���ɃW�����v */
        SYS_EXECDMP();
        break;
    default:
        if (code < 0) {
            for(;;);        /* ���̒l�̏ꍇ�͖������[�v */
        }
        restart();
        break;
    }
}


/* �ċN�����s�֐� */
static void restart(void)
{
    /* ���荞�݋֎~ */
    set_imask(15);

    /* �ċN���̎��s */
    execRestart();

    /*
     * �}���`�v���[���̋N��
     * �i�ċN�������s�����Ƃ��̂ݎ��s�����B�j
     */
    SYS_EXECDMP();
}


/* �ċN�����s�֐� */
static void execRestart(void)
{
    Sint32  ret;
    Sint32  timer;

    /* �f���f�����s���ȊO�Ȃ�}���`�v���[�������s */
    if (GFS_IS_DDS() == FALSE) {
        return;
    }

    /* �}�V���̏����� */
    initSaturn();

    /* CD�̃\�t�g���Z�b�g */
    if (resetCd() != OK) {
        return;
    }

    /* IP���[�h�̊J�n */
    ret = SYS_IPLGAMECD(0);
    if (ret != OK) {
        return;
    }

    /* IP�`�F�b�N���������� */
    execIpChkRun();
}


/* �}�V���̏����� */
static void initSaturn(void)
{
    /* SMPC�̏I���҂����X���[�uCPU�̒�~ */
    PER_SMPC_SSH_OFF();

    /* 26MHz�ɃN���b�N�`�F���W */
    SYS_CHGSYSCK(0);
}


/* CD�̃\�t�g���Z�b�g */
static Sint32 resetCd(void)
{
    /* �\�t�g���Z�b�g���I���҂� */
    if (CDC_CdInit(1, 0, 0, 0) != CDC_ERR_OK) {
        return NG;
    }
    if (waitHirq(CDC_HIRQ_ESEL) != OK) {
        return NG;
    }

    /* �`���I�ȃ��[�g�f�B���N�g���ւ̈ړ����� */
    chgDirRootDummy();

    return OK;
}


/* ���荞�ݗv�����W�X�^�̃r�b�g��1�ɂȂ�܂ő҂� */
static Sint32 waitHirq(Sint32 flag)
{
    Sint32  i;

    for (i = 0; i < TMOUT_HIRQREQ; i++) {
        if ((CDC_GetHirqReq() & flag) != 0) {
            return OK;
        }
    }
    return NG;
}


/* �`���I�ȃ��[�g�f�B���N�g���ւ̈ړ����� */
static void chgDirRootDummy(void)
{
    CDC_ChgDir(FLT_DUMMY, CDC_NUL_FID);

    CDC_AbortFile();

    return;
}


/* IP�`�F�b�N���������� */
/* �i�����ă��^�[�����Ȃ��֐��j */
static void execIpChkRun(void)
{
    /* �X�^�b�N�|�C���^�̕ۑ� */
    static Sint32   savesp;

    register Sint32 timer;
    register Sint32 ret;

    /* �X�^�b�N�̐ݒ� */
    savesp = setStackptr(STACKPTR);

    timer = 0;
    while (TRUE) {
        ret = SYS_RUNGAMECD(0);
        if (ret < 0) {
            break;
        }
        if (timer++ > TMOUT_IPLTSK) {
            break;
        }
    }

    /* �X�^�b�N��߂��i�s�v�ł��邪�O�̂��߁j */
    setStackptr(savesp);

    /* �}���`�v���[���̋N�� */
    SYS_EXECDMP();
}


/*
 * �X�^�b�N�̐ݒ�
 * [����]
 *      addr : �ݒ肷��X�^�b�N�|�C���^
 * [�֐��l]
 *      �ݒ�ύX�O�̃X�^�b�N�|�C���^�̒l��Ԃ�
 */
/*	1995-10-02	N.K	*/
#ifdef	__GNUC__

static Sint32 setStackptr(Sint32 addr)
{
	asm("  mov       r15, r0");  
    asm("  mov       r4, r15");
}
/*
 asm volatile (													\
"add %0,%2				\n"                                     \
"mov %2,r0				\n"										\
"and #0x80,r0			\n"										\
"or r0,%1				\n"										\
"cmp/pl %2				\n"										\
*/

#else

#pragma inline_asm(setStackptr)
static Sint32 setStackptr(Sint32 addr)
{
          MOV.L       R15, R0
          MOV.L       R4, R15
}

#endif

/* end of file */
