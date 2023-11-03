#include <yaul.h>
#include <stdint.h>
#include "shared.h"
#include "rom.h"
#include "perf.h"

#define ORDER_SYSTEM_CLIP_COORDS_INDEX 0
#define ORDER_LOCAL_COORDS_INDEX 1
#define ORDER_DRAW_START_INDEX (2)
#define ORDER_SMS_COUNT (64)
#define ORDER_DRAW_END_INDEX (ORDER_SMS_COUNT + ORDER_DRAW_START_INDEX)
#define ORDER_COUNT (ORDER_DRAW_END_INDEX + 1)

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 224

#define BG_PAL_ADDR (VDP2_CRAM_MODE_0_OFFSET(0, 0, 0))
#define SPR_PAL_ADDR (VDP2_CRAM_MODE_0_OFFSET(0, 8, 0))

#define BG_CELL_ADDR (VDP2_VRAM_ADDR(0, 0x00000))
#define BG_MAP_ADDR (VDP2_VRAM_ADDR(0, 0x08000))
#define NBG0_LINE_SCROLL VDP2_VRAM_ADDR(0, 0x04000)

#define RGB(r, g, b) (0x8000U | ((b) << 10) | ((g) << 5) | (r))

#define PERF_COUNTER(timer, code) \
    \ 
    perf_counter_start(&timer);   \
    do                            \
    {                             \
        code;                     \
    } while (0);                  \
    perf_counter_end(&timer);

uint16_t *ss_map = (uint16_t *)BG_MAP_ADDR;
uint16_t *colBgAddr = (uint16_t *)BG_PAL_ADDR; // NBGO0
uint16_t *colAddr = (uint16_t *)SPR_PAL_ADDR;  // VDP1

uint8_t *spr_tex_data;
uint8_t *bg_tex_data = (uint8_t *)BG_CELL_ADDR;
// RUN ON NBG0 ?

static int play = 0;

int sound = 1;
int hz = 60;

static vdp1_vram_partitions_t _vdp1_vram_partitions;
static vdp1_cmdt_list_t *_cmdt_list = NULL;
vdp1_cmdt_t *smsSprite;

vdp2_scrn_ls_h_t ls_tbl[0x10];

static uint8_t sms_input[2] = {0, 0};

void vdp1_sync_list()
{
}
void vdp1_clear_list()
{
}
uint8_t update_input()
{
    return sms_input[0];
}

uint8_t update_input2()
{
    return sms_input[1];
}

static uint8_t build_input(smpc_peripheral_digital_t *digital)
{
    uint8_t input = 0;
    if (digital->pressed.button.up)
        input |= INPUT_UP;
    if (digital->pressed.button.down)
        input |= INPUT_DOWN;
    if (digital->pressed.button.left)
        input |= INPUT_LEFT;
    if (digital->pressed.button.right)
        input |= INPUT_RIGHT;
    if (digital->pressed.button.a)
        input |= INPUT_BUTTON1;
    if (digital->pressed.button.b)
        input |= INPUT_BUTTON2;
    return input;
}

static void load_rom(void)
{
    uint32_t fileSize = sizeof(rom);
    make_name_lut();

    cart.rom = rom;
    cart.pages = fileSize / 0x4000;
}

static void wait_vblank()
{
    vdp1_sync_wait();
    _cmdt_list->count = ORDER_COUNT;
    vdp1_sync_cmdt_list_put(_cmdt_list, 0);
    vdp1_sync_render();
    vdp1_sync();

    vdp2_sync();
    vdp2_sync_wait();
}

const vdp2_scrn_ls_format_t ls_format = {
    .scroll_screen = VDP2_SCRN_NBG0,
    .table_base = NBG0_LINE_SCROLL,
    .interval = 0,
    .type = VDP2_SCRN_LS_TYPE_HORZ};

static void loop()
{
    perf_counter_t frame_time;
    perf_counter_init(&frame_time);

    perf_counter_t ls_time;
    perf_counter_init(&ls_time);

    smpc_peripheral_digital_t digital;
    while (1)
    {
        for (int i = 0; i < 256; i++)
            colAddr[i] = colBgAddr[i] = RGB(0, 0, 0); // palette2[0];

        memset(vdp.vram, 0, sizeof(vdp.vram));

        sn76496_init();
        sms_reset();
        load_rom();
        system_init(0);
        play = 1;

        uint8_t reg_fix_scroll = 0;

        while (play)
        {
            // Input handling
            smpc_peripheral_process();
            smpc_peripheral_digital_port(1, &digital);
            sms_input[0] = build_input(&digital);

            smpc_peripheral_digital_port(2, &digital);
            sms_input[1] = build_input(&digital);

            // update back color
            *(uint16_t *)VDP2_VRAM_ADDR(3, 0x01FFFE) = colBgAddr[0];

            // run 1 frame
            PERF_COUNTER(frame_time, {
                sms_frame(0);
            })

            // update x/y scrolling
            PERF_COUNTER(ls_time, {
                if (!(vdp.reg[0] & 0x40))
                    for (int i = 0; i < 0x10; i++)
                        ls_tbl[i].horz = (-vdp.scroll_x - 1) << 16;

                // todo disable ls if needed
                vdp2_scrn_ls_set(&ls_format);

                vdp_dma_enqueue((void *)NBG0_LINE_SCROLL,
                                ls_tbl,
                                sizeof(ls_tbl));
                vdp2_scrn_scroll_x_set(VDP2_SCRN_NBG0, vdp.scroll_x << 16);
                vdp2_scrn_scroll_y_set(VDP2_SCRN_NBG0, vdp.scroll_y << 16);
            })

            // debug
            dbgio_printf("[H[2J"
                         "frametime: %4lu\n"
                         "lstime: %4lu\n",
                         frame_time.ticks,
                         ls_time.ticks);
            dbgio_flush();
            wait_vblank();

            reg_fix_scroll = vdp.reg[0] & 0x40;
        }
    }
}

static void _cmdt_list_init(void)
{
    static const int16_vec2_t system_clip_coord =
        INT16_VEC2_INITIALIZER(SCREEN_WIDTH - 1,
                               SCREEN_HEIGHT - 1);
    static const int16_vec2_t local_coord_ul = INT16_VEC2_INITIALIZER(0, 0);

    _cmdt_list = vdp1_cmdt_list_alloc(ORDER_COUNT);

    (void)memset(&_cmdt_list->cmdts[0], 0x00, sizeof(vdp1_cmdt_t) * ORDER_COUNT);

    _cmdt_list->count = ORDER_COUNT;

    vdp1_cmdt_t *cmdts;
    cmdts = &_cmdt_list->cmdts[0];

    vdp1_cmdt_system_clip_coord_set(&cmdts[ORDER_SYSTEM_CLIP_COORDS_INDEX]);
    vdp1_cmdt_vtx_system_clip_coord_set(&cmdts[ORDER_SYSTEM_CLIP_COORDS_INDEX],
                                        system_clip_coord);

    vdp1_cmdt_local_coord_set(&cmdts[ORDER_LOCAL_COORDS_INDEX]);
    vdp1_cmdt_vtx_local_coord_set(&cmdts[ORDER_LOCAL_COORDS_INDEX], local_coord_ul);

    vdp1_cmdt_end_set(&cmdts[ORDER_DRAW_START_INDEX]);
    vdp1_cmdt_end_set(&cmdts[ORDER_DRAW_END_INDEX]);
    vdp1_sync_cmdt_list_put(_cmdt_list, 0);

    smsSprite = &_cmdt_list->cmdts[ORDER_DRAW_START_INDEX];
}

const vdp2_scrn_cell_format_t format = {
    .scroll_screen = VDP2_SCRN_NBG0,
    .ccc = VDP2_SCRN_CCC_PALETTE_16,
    .char_size = VDP2_SCRN_CHAR_SIZE_1X1,
    .pnd_size = 1,
    .aux_mode = VDP2_SCRN_AUX_MODE_0,
    .plane_size = VDP2_SCRN_PLANE_SIZE_1X1,
    .cpd_base = (uint32_t)BG_CELL_ADDR,
    .palette_base = (uint32_t)BG_PAL_ADDR};

const vdp2_scrn_normal_map_t normal_map = {
    .plane_a = (vdp2_vram_t)BG_MAP_ADDR,
    .plane_b = (vdp2_vram_t)BG_MAP_ADDR,
    .plane_c = (vdp2_vram_t)BG_MAP_ADDR,
    .plane_d = (vdp2_vram_t)BG_MAP_ADDR};

void main()
{

    dbgio_init();
    dbgio_dev_default_init(DBGIO_DEV_VDP2);
    dbgio_dev_font_load();

    perf_init();

    _cmdt_list_init();

    vdp2_scrn_ls_set(&ls_format);
    make_cram_lut();
    make_bp_lut();

    loop();
}

static void vdp2_init()
{
    const vdp2_vram_cycp_t vram_cycp = {
        .pt[0].t0 = VDP2_VRAM_CYCP_PNDR_NBG0,
        .pt[0].t1 = VDP2_VRAM_CYCP_CPU_RW,
        .pt[0].t2 = VDP2_VRAM_CYCP_CPU_RW,
        .pt[0].t3 = VDP2_VRAM_CYCP_CPU_RW,
        .pt[0].t4 = VDP2_VRAM_CYCP_CHPNDR_NBG0,
        .pt[0].t5 = VDP2_VRAM_CYCP_CPU_RW,
        .pt[0].t6 = VDP2_VRAM_CYCP_CPU_RW,
        .pt[0].t7 = VDP2_VRAM_CYCP_CPU_RW,

        .pt[1].t0 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t1 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t2 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t3 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[1].t7 = VDP2_VRAM_CYCP_NO_ACCESS,

        .pt[2].t0 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t1 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t2 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t3 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[2].t7 = VDP2_VRAM_CYCP_NO_ACCESS,

        .pt[3].t0 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t1 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t2 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t3 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t4 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t5 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t6 = VDP2_VRAM_CYCP_NO_ACCESS,
        .pt[3].t7 = VDP2_VRAM_CYCP_NO_ACCESS};

    vdp2_vram_cycp_set(&vram_cycp);

    vdp2_scrn_cell_format_set(&format, &normal_map);
    vdp2_scrn_priority_set(VDP2_SCRN_NBG0, 5);
    vdp2_scrn_display_set(VDP2_SCRN_DISPTP_NBG0);

    vdp2_cram_mode_set(1);
}

static void _vblank_out_handler(void *work __unused)
{
    smpc_peripheral_intback_issue();
}

void user_init(void)
{
    smpc_peripheral_init();

    // VDP2
    vdp2_vram_cycp_clear();

    vdp2_tvmd_display_res_set(VDP2_TVMD_INTERLACE_NONE, VDP2_TVMD_HORZ_NORMAL_A,
                              VDP2_TVMD_VERT_224);
    vdp2_scrn_back_color_set(VDP2_VRAM_ADDR(3, 0x01FFFE),
                             RGB1555(0, 0, 3, 15));
    vdp2_sprite_priority_set(0, 6);

    vdp2_init();

    // VDP1
    vdp1_env_t env;
    vdp1_env_default_init(&env);

    env.erase_color = RGB1555(0, 0, 0, 0);

    vdp1_env_set(&env);

    vdp_sync_vblank_out_set(_vblank_out_handler, NULL);

    vdp1_vram_partitions_set(ORDER_COUNT, 0x8000, 0, 0x200);
    vdp1_vram_partitions_get(&_vdp1_vram_partitions);

    spr_tex_data = _vdp1_vram_partitions.texture_base;
    // colAddr = _vdp1_vram_partitions.clut_base;

    // sync
    vdp2_tvmd_display_set();
    vdp2_sync();
    vdp2_sync_wait();
}