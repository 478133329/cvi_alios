#ifndef __GC4023_CMOS_PARAM_H_
#define __GC4023_CMOS_PARAM_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#include "cvi_comm_cif.h"
#include "cvi_type.h"
#include "cvi_sns_ctrl.h"
#include "gc4023_cmos_ex.h"

static const GC4023_MODE_S g_astGc4023_mode[GC4023_MODE_NUM] = {
	[GC4023_MODE_2560X1440P30] = {
		.name = "2560X1440P30",
		.astImg[0] = {
			.stSnsSize = {
				.u32Width = 2560,
				.u32Height = 1440,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 2560,
				.u32Height = 1440,
			},
			.stMaxSize = {
				.u32Width = 2560,
				.u32Height = 1440,
			},
		},
		.f32MaxFps = 30,
		.f32MinFps = 5.49, /* 1500 * 30 / 8191*/
		.u32HtsDef = 2400,
		.u32VtsDef = 1500,
		.stExp[0] = {
			.u16Min = 1,
			.u16Max = 0x3fff,
			.u16Def = 100,
			.u16Step = 1,
		},
		.stAgain[0] = {
			.u32Min = 1024,
			.u32Max = 16*3805,
			.u32Def = 1024,
			.u32Step = 1,
		},
		.stDgain[0] = {
			.u32Min = 1024,
			.u32Max = 1024,
			.u32Def = 1024,
			.u32Step = 1,
		},
	},
};

static ISP_CMOS_NOISE_CALIBRATION_S g_stIspNoiseCalibratio = {.CalibrationCoef = {
	{	//iso  100
		{0.04006369039416313171,	5.73531675338745117188}, //B: slope, intercept
		{0.03064448758959770203,	8.56994438171386718750}, //Gb: slope, intercept
		{0.03068985790014266968,	8.82220363616943359375}, //Gr: slope, intercept
		{0.03361049667000770569,	7.20687675476074218750}, //R: slope, intercept
	},
	{	//iso  200
		{0.05054308474063873291,	7.91628408432006835938}, //B: slope, intercept
		{0.03632996603846549988,	13.05291843414306640625}, //Gb: slope, intercept
		{0.03697143867611885071,	12.49540805816650390625}, //Gr: slope, intercept
		{0.04156460240483283997,	10.44298553466796875000}, //R: slope, intercept
	},
	{	//iso  400
		{0.06805337965488433838,	11.55693244934082031250}, //B: slope, intercept
		{0.04390667378902435303,	18.59673881530761718750}, //Gb: slope, intercept
		{0.04630871489644050598,	17.90183258056640625000}, //Gr: slope, intercept
		{0.05292420834302902222,	15.17318439483642578125}, //R: slope, intercept
	},
	{	//iso  800
		{0.08635756373405456543,	17.79124641418457031250}, //B: slope, intercept
		{0.05776864662766456604,	26.76541900634765625000}, //Gb: slope, intercept
		{0.05839699879288673401,	26.44194030761718750000}, //Gr: slope, intercept
		{0.06750740110874176025,	22.42554473876953125000}, //R: slope, intercept
	},
	{	//iso  1600
		{0.12254384160041809082,	26.20610046386718750000}, //B: slope, intercept
		{0.07916855812072753906,	39.39273834228515625000}, //Gb: slope, intercept
		{0.07857896387577056885,	39.03499984741210937500}, //Gr: slope, intercept
		{0.09273355454206466675,	33.09597778320312500000}, //R: slope, intercept
	},
	{	//iso  3200
		{0.18002016842365264893,	34.86975860595703125000}, //B: slope, intercept
		{0.10951708257198333740,	54.58878326416015625000}, //Gb: slope, intercept
		{0.10485322028398513794,	57.16654205322265625000}, //Gr: slope, intercept
		{0.13257601857185363770,	46.27093505859375000000}, //R: slope, intercept
	},
	{	//iso  6400
		{0.24713687598705291748,	48.62341690063476562500}, //B: slope, intercept
		{0.14974890649318695068,	77.06428527832031250000}, //Gb: slope, intercept
		{0.14544390141963958740,	76.57913970947265625000}, //Gr: slope, intercept
		{0.19056233763694763184,	62.13500213623046875000}, //R: slope, intercept
	},
	{	//iso  12800
		{0.37728109955787658691,	58.15543365478515625000}, //B: slope, intercept
		{0.20440576970577239990,	100.45700073242187500000}, //Gb: slope, intercept
		{0.20059910416603088379,	102.35488891601562500000}, //Gr: slope, intercept
		{0.27388775348663330078,	79.65499877929687500000}, //R: slope, intercept
	},
	{	//iso  25600
		{0.36612421274185180664,	115.28938293457031250000}, //B: slope, intercept
		{0.22633622586727142334,	164.58416748046875000000}, //Gb: slope, intercept
		{0.21590474247932434082,	168.92042541503906250000}, //Gr: slope, intercept
		{0.33193346858024597168,	127.92090606689453125000}, //R: slope, intercept
	},
	{	//iso  51200
		{0.48242908716201782227,	147.39015197753906250000}, //B: slope, intercept
		{0.28994381427764892578,	223.02711486816406250000}, //Gb: slope, intercept
		{0.29200506210327148438,	220.64030456542968750000}, //Gr: slope, intercept
		{0.42304891347885131836,	173.74638366699218750000}, //R: slope, intercept
	},
	{	//iso  102400
		{0.62099909782409667969,	130.97862243652343750000}, //B: slope, intercept
		{0.39534106850624084473,	219.74490356445312500000}, //Gb: slope, intercept
		{0.39458695054054260254,	213.37374877929687500000}, //Gr: slope, intercept
		{0.55690109729766845703,	158.37773132324218750000}, //R: slope, intercept
	},
	{	//iso  204800
		{0.75350415706634521484,	77.81707000732421875000}, //B: slope, intercept
		{0.52716732025146484375,	148.77879333496093750000}, //Gb: slope, intercept
		{0.51073729991912841797,	153.86495971679687500000}, //Gr: slope, intercept
		{0.68910604715347290039,	102.12422180175781250000}, //R: slope, intercept
	},
	{	//iso  409600
		{0.90276730060577392578,	43.78258514404296875000}, //B: slope, intercept
		{0.62851423025131225586,	119.41429138183593750000}, //Gb: slope, intercept
		{0.64918899536132812500,	110.74241638183593750000}, //Gr: slope, intercept
		{0.80880594253540039063,	68.89983367919921875000}, //R: slope, intercept
	},
	{	//iso  819200
		{0.90276730060577392578,	43.78258514404296875000}, //B: slope, intercept
		{0.62851423025131225586,	119.41429138183593750000}, //Gb: slope, intercept
		{0.64918899536132812500,	110.74241638183593750000}, //Gr: slope, intercept
		{0.80880594253540039063,	68.89983367919921875000}, //R: slope, intercept
	},
	{	//iso  1638400
		{0.90276730060577392578,	43.78258514404296875000}, //B: slope, intercept
		{0.62851423025131225586,	119.41429138183593750000}, //Gb: slope, intercept
		{0.64918899536132812500,	110.74241638183593750000}, //Gr: slope, intercept
		{0.80880594253540039063,	68.89983367919921875000}, //R: slope, intercept
	},
	{	//iso  3276800
		{0.90276730060577392578,	43.78258514404296875000}, //B: slope, intercept
		{0.62851423025131225586,	119.41429138183593750000}, //Gb: slope, intercept
		{0.64918899536132812500,	110.74241638183593750000}, //Gr: slope, intercept
		{0.80880594253540039063,	68.89983367919921875000}, //R: slope, intercept
	},
} };

static ISP_CMOS_BLACK_LEVEL_S g_stIspBlcCalibratio = {
	.bUpdate = CVI_TRUE,
	.blcAttr = {
		.Enable = 1,
		.enOpType = OP_TYPE_AUTO,
		.stManual = {252, 252, 252, 252, 0, 0, 0, 0
#ifdef ARCH_CV182X
		, 1091, 1091, 1091, 1091
#endif
		},
		.stAuto = {
			{252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252 },
			{252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252 },
			{252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252 },
			{252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252 },
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
#ifdef ARCH_CV182X
			{1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091,
				1091, 1091},
			{1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091,
				1091, 1091},
			{1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091,
				1091, 1091},
			{1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091, 1091,
				1091, 1091},
#endif
		},
	},
};

struct combo_dev_attr_s gc4023_rx_attr = {
	.input_mode = INPUT_MODE_MIPI,
	.mac_clk = RX_MAC_CLK_200M,
	.mipi_attr = {
		.raw_data_type = RAW_DATA_10BIT,
		.lane_id = {2, 1, 3, -1, -1},
		.pn_swap = {1, 1, 1, 0, 0},
		.wdr_mode = CVI_MIPI_WDR_MODE_NONE,
	},
	.mclk = {
		.cam = 0,
		.freq = CAMPLL_FREQ_27M,
	},
	.devno = 0,
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* __GC4023_CMOS_PARAM_H_ */

