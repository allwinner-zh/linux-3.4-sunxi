#include "de_dsi.h"
#include <asm/cacheflush.h>

int (*dsi_main)(int type, uintptr_t* para) = NULL;

int bsp_dsi_main(unsigned int type, uintptr_t* para)
{
	int ret = -1;

	if (dsi_main)
		ret = dsi_main(type, para);

	return ret;
}

int dsi_set_print_func(dsi_print print)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)print;
	return bsp_dsi_main(BSP_DSI_SET_PRINT_FUNC, para);
}

int dsi_set_print_val_func(dsi_print_val print_val)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)print_val;
	return bsp_dsi_main(BSP_DSI_SET_PRINTF_VAL_FUNC, para);
}

s32 dsi_init(struct dsi_init_para* init_para)
{
	uintptr_t para[8];

	memcpy((void *)DSI_MC_START, (void *)dsi_mc_table, dsi_mc_table_size);
	__cpuc_flush_dcache_area((void *)DSI_MC_START, dsi_mc_table_size);
	__cpuc_flush_icache_all();
	dsi_main = (int (*)(int, uintptr_t*))DSI_MC_START;

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)init_para;
	return bsp_dsi_main(BSP_DSI_INIT, para);
}

s32 dsi_set_reg_base(u32 sel, u32 base)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)base;
	return bsp_dsi_main(BSP_DSI_SET_REG_BASE, para);
}

u32 dsi_get_reg_base(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_GET_REG_BASE, para);
}

u32 dsi_irq_query(u32 sel,__dsi_irq_id_t id)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)id;
	return bsp_dsi_main(BSP_DSI_IRQ_QUERY, para);
}

s32 dsi_cfg(u32 sel,disp_panel_para * panel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)panel;
	return bsp_dsi_main(BSP_DSI_CFG, para);
}

s32 dsi_exit(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_EXIT, para);
}

s32 dsi_open(u32 sel,disp_panel_para * panel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)panel;
	return bsp_dsi_main(BSP_DSI_OPEN, para);
}

s32 dsi_close(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_CLOSE, para);
}

s32 dsi_inst_busy(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_INST_BUSY, para);
}

s32 dsi_tri_start(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_TRI_START, para);
}

s32 dsi_dcs_wr(u32 sel,u32 cmd,u8* para_p,u32 para_num)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para_p;
	para[3] = (uintptr_t)para_num;
	return bsp_dsi_main(BSP_DSI_DCS_WR, para);
}

s32 dsi_dcs_wr_index(u32 sel,u8 index)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)index;
	return bsp_dsi_main(BSP_DSI_DCS_WR_INDEX, para);
}

s32 dsi_dcs_wr_data(u32 sel,u8 data)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)data;
	return bsp_dsi_main(BSP_DSI_DCS_WR_DATA, para);
}

u32 dsi_get_start_delay(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_GET_START_DELAY, para);
}

u32 dsi_get_cur_line(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_GET_CUR_LINE, para);
}

u32 dsi_io_open(u32 sel,disp_panel_para * panel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)panel;
	return bsp_dsi_main(BSP_DSI_IO_OPEN, para);
}

u32 dsi_io_close(u32 sel)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	return bsp_dsi_main(BSP_DSI_IO_CLOSE, para);
}

s32 dsi_clk_enable(u32 sel, u32 en)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)en;
	return bsp_dsi_main(BSP_DSI_CLK_ENABLE, para);
}

s32 dsi_dcs_wr_0para(u32 sel,u8 cmd)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	return bsp_dsi_main(BSP_DSI_DCS_WR_0PARA, para);
}

s32 dsi_dcs_wr_1para(u32 sel,u8 cmd,u8 data)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)data;
	return bsp_dsi_main(BSP_DSI_DCS_WR_1PARA, para);
}

s32 dsi_dcs_wr_2para(u32 sel,u8 cmd,u8 para1,u8 para2)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	return bsp_dsi_main(BSP_DSI_DCS_WR_2PARA, para);
}

s32 dsi_dcs_wr_3para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	return bsp_dsi_main(BSP_DSI_DCS_WR_3PARA, para);
}

s32 dsi_dcs_wr_4para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3,u8 para4)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	para[5] = (uintptr_t)para4;
	return bsp_dsi_main(BSP_DSI_DCS_WR_4PARA, para);
}

s32 dsi_dcs_wr_5para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3,u8 para4,u8 para5)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	para[5] = (uintptr_t)para4;
	para[6] = (uintptr_t)para5;
	return bsp_dsi_main(BSP_DSI_DCS_WR_5PARA, para);
}

s32 dsi_gen_wr_0para(u32 sel,u8 cmd)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	return bsp_dsi_main(BSP_DSI_GEN_WR_0PARA, para);
}

s32 dsi_gen_wr_1para(u32 sel,u8 cmd,u8 data)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)data;
	return bsp_dsi_main(BSP_DSI_GEN_WR_1PARA, para);
}

s32 dsi_gen_wr_2para(u32 sel,u8 cmd,u8 para1,u8 para2)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	return bsp_dsi_main(BSP_DSI_GEN_WR_2PARA, para);
}

s32 dsi_gen_wr_3para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	return bsp_dsi_main(BSP_DSI_GEN_WR_3PARA, para);
}

s32 dsi_gen_wr_4para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3,u8 para4)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	para[5] = (uintptr_t)para4;
	return bsp_dsi_main(BSP_DSI_GEN_WR_4PARA, para);
}

s32 dsi_gen_wr_5para(u32 sel,u8 cmd,u8 para1,u8 para2,u8 para3,u8 para4,u8 para5)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para1;
	para[3] = (uintptr_t)para2;
	para[4] = (uintptr_t)para3;
	para[5] = (uintptr_t)para4;
	para[6] = (uintptr_t)para5;
	return bsp_dsi_main(BSP_DSI_GEN_WR_5PARA, para);
}

s32 dsi_dcs_rd(u32 sel,u8 cmd,u8* para_p,u32* num_p)
{
	uintptr_t para[8];

	memset(para, 0, 8*sizeof(uintptr_t));
	para[0] = (uintptr_t)sel;
	para[1] = (uintptr_t)cmd;
	para[2] = (uintptr_t)para_p;
	return bsp_dsi_main(BSP_DSI_DCS_RD, para);
}

EXPORT_SYMBOL(dsi_dcs_wr);
EXPORT_SYMBOL(dsi_dcs_wr_0para);
EXPORT_SYMBOL(dsi_dcs_wr_1para);
EXPORT_SYMBOL(dsi_dcs_wr_2para);
EXPORT_SYMBOL(dsi_dcs_wr_3para);
EXPORT_SYMBOL(dsi_dcs_wr_4para);
EXPORT_SYMBOL(dsi_dcs_wr_5para);

EXPORT_SYMBOL(dsi_gen_wr_0para);
EXPORT_SYMBOL(dsi_gen_wr_1para);
EXPORT_SYMBOL(dsi_gen_wr_2para);
EXPORT_SYMBOL(dsi_gen_wr_3para);
EXPORT_SYMBOL(dsi_gen_wr_4para);
EXPORT_SYMBOL(dsi_gen_wr_5para);
EXPORT_SYMBOL(dsi_dcs_rd);