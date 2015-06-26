#ifndef __de_dsi_h__
#define __de_dsi_h__

#include "ebios_lcdc_tve.h"

#define DSI_MC_START                   (0xCB213000)
#define BSP_DSI_SET_PRINT_FUNC         (0x01)
#define BSP_DSI_SET_PRINTF_VAL_FUNC    (0x02)
#define BSP_DSI_INIT                   (0x03)
#define BSP_DSI_SET_REG_BASE           (0x04)
#define BSP_DSI_GET_REG_BASE           (0x05)
#define BSP_DSI_IRQ_QUERY              (0x06)
#define BSP_DSI_CFG                    (0x07)
#define BSP_DSI_EXIT                   (0x08)
#define BSP_DSI_OPEN                   (0x09)
#define BSP_DSI_CLOSE                  (0x0a)
#define BSP_DSI_INST_BUSY              (0x0b)
#define BSP_DSI_TRI_START              (0x0c)
#define BSP_DSI_DCS_WR                 (0x0d)
#define BSP_DSI_DCS_WR_INDEX           (0x0e)
#define BSP_DSI_DCS_WR_DATA            (0x0f)
#define BSP_DSI_GET_START_DELAY        (0x11)
#define BSP_DSI_GET_CUR_LINE           (0x12)
#define BSP_DSI_IO_OPEN                (0x13)
#define BSP_DSI_IO_CLOSE               (0x14)
#define BSP_DSI_CLK_ENABLE             (0x15)
#define BSP_DSI_DCS_WR_0PARA           (0x16)
#define BSP_DSI_DCS_WR_1PARA           (0x17)
#define BSP_DSI_DCS_WR_2PARA           (0x18)
#define BSP_DSI_DCS_WR_3PARA           (0x19)
#define BSP_DSI_DCS_WR_4PARA           (0x1a)
#define BSP_DSI_DCS_WR_5PARA           (0x1b)
#define BSP_DSI_GEN_WR_0PARA           (0x1c)
#define BSP_DSI_GEN_WR_1PARA           (0x1d)
#define BSP_DSI_GEN_WR_2PARA           (0x1e)
#define BSP_DSI_GEN_WR_3PARA           (0x1f)
#define BSP_DSI_GEN_WR_4PARA           (0x21)
#define BSP_DSI_GEN_WR_5PARA           (0x22)
#define BSP_DSI_DCS_RD                 (0x23)

extern unsigned char dsi_mc_table[];
extern unsigned int dsi_mc_table_size;

struct dsi_init_para
{
	s32 (*delay_ms)(u32 ms);
	s32 (*delay_us)(u32 us);
};

typedef void (*dsi_print)(const char *c);
typedef void (*dsi_print_val)(u32 val);

extern int dsi_set_print_func(dsi_print print);
extern int dsi_set_print_val_func(dsi_print_val print_val);
s32 dsi_init(struct dsi_init_para* para);

s32 dsi_set_reg_base(u32 sel, u32 base);
u32 dsi_get_reg_base(u32 sel);
u32 dsi_irq_query(u32 sel,__dsi_irq_id_t id);
s32 dsi_cfg(u32 sel,disp_panel_para * panel);
s32 dsi_exit(u32 sel);
s32 dsi_open(u32 sel,disp_panel_para * panel);
s32 dsi_close(u32 sel);
s32 dsi_inst_busy(u32 sel);
s32 dsi_tri_start(u32 sel);
s32 dsi_dcs_wr(u32 sel,__u32 cmd,u8* para_p,u32 para_num);
s32 dsi_dcs_wr_index(u32 sel,u8 index);
s32 dsi_dcs_wr_data(u32 sel,u8 data);
u32 dsi_get_start_delay(u32 sel);
u32 dsi_get_cur_line(u32 sel);
u32 dsi_io_open(u32 sel,disp_panel_para * panel);
u32 dsi_io_close(u32 sel);
s32 dsi_clk_enable(u32 sel, u32 en);

s32 dsi_dcs_wr_0para(u32 sel, u8 cmd);
s32 dsi_dcs_wr_1para(u32 sel, u8 cmd, u8 para);
s32 dsi_dcs_wr_2para(u32 sel, u8 cmd, u8 para1, u8 para2);
s32 dsi_dcs_wr_3para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3);
s32 dsi_dcs_wr_4para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3, u8 para4);
s32 dsi_dcs_wr_5para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3, u8 para4, u8 para5);

s32 dsi_gen_wr_0para(u32 sel, u8 cmd);
s32 dsi_gen_wr_1para(u32 sel, u8 cmd, u8 para);
s32 dsi_gen_wr_2para(u32 sel, u8 cmd, u8 para1, u8 para2);
s32 dsi_gen_wr_3para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3);
s32 dsi_gen_wr_4para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3, u8 para4);
s32 dsi_gen_wr_5para(u32 sel, u8 cmd, u8 para1, u8 para2, u8 para3, u8 para4, u8 para5);

s32 dsi_dcs_rd(u32 sel,u8 cmd,u8* para_p,u32* num_p);

#endif
