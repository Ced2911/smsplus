ifeq ($(strip $(YAUL_INSTALL_ROOT)),)
  $(error Undefined YAUL_INSTALL_ROOT (install root directory))
endif

include $(YAUL_INSTALL_ROOT)/share/build.pre.mk

# Each asset follows the format: <path>;<symbol>. Duplicates are removed
BUILTIN_ASSETS=

SH_PROGRAM:= smsplus

CSZ80:=				\
	cz80/cz80.c		\
	cz80/intf.c

RAZE:= 				\
	raze/z80intrf.c \
	raze/raze.sx

SH_SRCS:= 			\
	system.c 		\
	sms.c 			\
	vdp.c 			\
	saturn/yaul.c 	\
	saturn/perf.c 	\
	$(RAZE)

SH_CFLAGS+= -Os -I. -g
SH_LDFLAGS+=

IP_VERSION:= V1.000
IP_RELEASE_DATE:= 20160101
IP_AREAS:= JTUBKAEL
IP_PERIPHERALS:= JAMKST
IP_TITLE:= SMSPLUS
IP_MASTER_STACK_ADDR:= 0x06004000
IP_SLAVE_STACK_ADDR:= 0x06001E00
IP_1ST_READ_ADDR:= 0x06004000
IP_1ST_READ_SIZE:= 0

include $(YAUL_INSTALL_ROOT)/share/build.post.iso-cue.mk


size: $(SH_BUILD_PATH)/$(SH_PROGRAM).elf
	$(SH_NM) --print-size  $(SH_BUILD_PATH)/$(SH_PROGRAM).elf > $(SH_BUILD_PATH)/${SH_PROGRAM}.size.txt