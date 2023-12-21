#include "app_param.h"
#include "custom_param.h"
#include "minIni.h"
#include "sensor_cfg.h"
#include "cvi_ipcm.h"
#include "drv/tick.h"

#define APP_PARAM_STRING_NAME_LEN 64
#define DUMP_DEBUG 0

typedef struct _PARTITION_CHECK_HAED_S
{
    unsigned int magic_number;
    unsigned int crc;
    unsigned int header_ver;
    unsigned int length;
    unsigned int reserved;
    unsigned int package_number;
    unsigned int package_length[10];
}PARTITION_CHECK_HAED_S;

static int _param_check_head(unsigned char * buffer)
{
    PARTITION_CHECK_HAED_S * pstParam = (PARTITION_CHECK_HAED_S *)(buffer);
    if (buffer == NULL) {
        return -1;
    }
    if (pstParam->magic_number != 0xDEADBEEF) {
        return -1;
    }
    return 0;
}

static unsigned int stringHash(char * str)
{
    unsigned int hash = 5381;
    while(*str) {
        hash += (hash << 5) + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

unsigned int as32SensorType[SNS_TYPE_WDR_BUTT] = {0};
unsigned int as32PixFormat[PIXEL_FORMAT_MAX] = {0};
unsigned int as32ViVpssMode[VI_VPSS_MODE_BUTT] = {0};
unsigned int as32VpssMode[VPSS_MODE_BUTT] = {0};
unsigned int as32VpssAenInput[VPSS_INPUT_BUTT] = {0};
unsigned int as32ViWdrMode[WDR_MODE_MAX] = {0};
unsigned int as32CompressMode[COMPRESS_MODE_BUTT] = {0};
unsigned int as32Mclk[RX_MAC_CLK_BUTT] = {0};
unsigned int as32MclkFreq[CAMPLL_FREQ_NUM] = {0};

PARAM_SYS_CFG_S stSysCtx = {0};
PARAM_VI_CFG_S stViCtx = {0};

static int parse_SysParam(char * buffer)
{
    char _section[64] = {0};
    char _stringPrase[64] = {0};
    long _vpss_mode = 0;
    stSysCtx.u8ViCnt = 1; // default 1
    if (buffer == NULL ) {
        return -1;
    }
    ini_gets("vpss_mode", "enMode", "", _stringPrase, sizeof(_stringPrase), buffer);
    _vpss_mode = stringHash(_stringPrase);
    for (int i = 0; i < sizeof(as32VpssMode)/sizeof(as32VpssMode[0]); i++) {
        if (_vpss_mode == as32VpssMode[i]) {
            stSysCtx.stVPSSMode.enMode = i;
            break;
        }
    }
    for (int i = 0; i < 2 ; i++) {
        snprintf(_section, sizeof(_section), "vpss_dev%d", i);
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "aenInput", "", _stringPrase, sizeof(_stringPrase), buffer);
        long aenInput = stringHash(_stringPrase);
        for (int j = 0; j < sizeof(as32VpssAenInput)/sizeof(as32VpssAenInput[0]); j++) {
            if (aenInput == as32VpssAenInput[j]) {
                stSysCtx.stVPSSMode.aenInput[i] = j;
                if ( j >= 1) {
                    stSysCtx.u8ViCnt = 2;
                }
            }
        }
        stSysCtx.stVPSSMode.ViPipe[i] = ini_getl(_section, "ViPipe", 0, buffer);
    }

    {
        //vi_vpss_mode prase
        memset(_stringPrase, 0, sizeof(_stringPrase));
        ini_gets("vi_vpss_mode", "enMode", "", _stringPrase, sizeof(_stringPrase), buffer);
        long vi_vpss_mode = stringHash(_stringPrase);
        for (int i = 0; i < sizeof(as32ViVpssMode)/sizeof(as32ViVpssMode[0]); i++) {
            if (vi_vpss_mode == as32ViVpssMode[i]) {
                stSysCtx.stVIVPSSMode.aenMode[0] = i;
                if (stSysCtx.stVPSSMode.enMode == VPSS_MODE_DUAL) {
                    stSysCtx.stVIVPSSMode.aenMode[1] = i;
                }
                break;
            }
        }
    }

    //vi_vpss_mode_%d prase
    for (int j = 0; j < CVI_MAX_SENSOR_NUM; j++) {
        snprintf(_section, sizeof(_section), "vi_vpss_mode_%d", j);
        memset(_stringPrase, 0, sizeof(_stringPrase));
        ini_gets(_section, "enMode", "", _stringPrase, sizeof(_stringPrase), buffer);
        long vi_vpss_mode = stringHash(_stringPrase);
        for (int i = 0; i < sizeof(as32ViVpssMode)/sizeof(as32ViVpssMode[0]); i++) {
            if (vi_vpss_mode == as32ViVpssMode[i]) {
                stSysCtx.stVIVPSSMode.aenMode[j] = i;
                if(j > 0) {
                    stSysCtx.u8ViCnt = 2;
                }
            }
        }
    }

    //vb param
    stSysCtx.u8VbPoolCnt = ini_getl("vb_config", "vb_pool_cnt", 0, buffer);
    if (stSysCtx.u8VbPoolCnt != 0) {
        stSysCtx.pstVbPool = (PARAM_VB_CFG_S *)malloc(sizeof(PARAM_VB_CFG_S)*stSysCtx.u8VbPoolCnt);
    }
    for (int i = 0; i < stSysCtx.u8VbPoolCnt; i++) {
        snprintf(_section, sizeof(_section), "vb_pool_%d", i);
        stSysCtx.pstVbPool[i].u16width = ini_getl(_section, "frame_width", 0, buffer);
        stSysCtx.pstVbPool[i].u16height = ini_getl(_section, "frame_height", 0, buffer);
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "compress_mode", "", _stringPrase, sizeof(_stringPrase), buffer);
        long _compress_mode = stringHash(_stringPrase);
        for (unsigned int j = 0; j < sizeof(as32CompressMode)/sizeof(as32CompressMode[0]); j++) {
            if (_compress_mode == as32CompressMode[j]) {
                stSysCtx.pstVbPool[i].enCmpMode = j;
                break;
            }
        }
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "frame_fmt", "", _stringPrase, sizeof(_stringPrase), buffer);
        long _frame_fmt = stringHash(_stringPrase);
        for (unsigned int j = 0; j < sizeof(as32PixFormat)/sizeof(as32PixFormat[0]); j++) {
            if (_frame_fmt == as32PixFormat[j]) {
                stSysCtx.pstVbPool[i].fmt = j;
                break;
            }
        }
        stSysCtx.pstVbPool[i].enBitWidth = DATA_BITWIDTH_8;//default DATA_BITWIDTH_8
        stSysCtx.pstVbPool[i].u8VbBlkCnt = ini_getl(_section, "blk_cnt", 0, buffer);
    }
#if (DUMP_DEBUG == 1)
    aos_debug_printf("The stSysCtx.u8VbPoolCnt is %d \r\n", stSysCtx.u8VbPoolCnt);
    aos_debug_printf("The stSysCtx.u8ViCnt is %d \r\n", stSysCtx.u8ViCnt);
    aos_debug_printf("The stSysCtx.stVIVPSSMode.aenMode[0] is %d \r\n", stSysCtx.stVIVPSSMode.aenMode[0]);
    aos_debug_printf("The stSysCtx.stVIVPSSMode.aenMode[1] is %d \r\n", stSysCtx.stVIVPSSMode.aenMode[1]);
    aos_debug_printf("The stSysCtx.stVPSSMode.ViPipe[0] is %d \r\n", stSysCtx.stVPSSMode.ViPipe[0]);
    aos_debug_printf("The stSysCtx.stVPSSMode.ViPipe[1] is %d \r\n", stSysCtx.stVPSSMode.ViPipe[1]);
    aos_debug_printf("The stSysCtx.stVPSSMode.enMode is %d \r\n", stSysCtx.stVPSSMode.enMode);
    aos_debug_printf("The stSysCtx.stVPSSMode.aenInput[0] is %d \r\n", stSysCtx.stVPSSMode.aenInput[0]);
    aos_debug_printf("The stSysCtx.stVPSSMode.aenInput[1] is %d \r\n", stSysCtx.stVPSSMode.aenInput[1]);
    for (int i = 0 ; i < stSysCtx.u8VbPoolCnt; i++) {
        aos_debug_printf("stSysCtx.pstVbPool[%d].u16width %d \r\n", i,stSysCtx.pstVbPool[i].u16width);
        aos_debug_printf("stSysCtx.pstVbPool[%d].u16height %d \r\n", i,stSysCtx.pstVbPool[i].u16height);
        aos_debug_printf("stSysCtx.pstVbPool[%d].fmt %d \r\n", i,stSysCtx.pstVbPool[i].fmt);
        aos_debug_printf("stSysCtx.pstVbPool[%d].enBitWidth %d \r\n", i,stSysCtx.pstVbPool[i].enBitWidth);
        aos_debug_printf("stSysCtx.pstVbPool[%d].enCmpMode %d \r\n", i,stSysCtx.pstVbPool[i].enCmpMode);
        aos_debug_printf("stSysCtx.pstVbPool[%d].u8VbBlkCnt %d \r\n", i,stSysCtx.pstVbPool[i].u8VbBlkCnt);
    }
#endif

    return 0;
}

static int parse_ViParam(char * buffer)
{
    char _section[64] = {0};
    char _key[64] = {0};
    char _stringPrase[64] = {0};
    unsigned int * phead = (unsigned int *)buffer;
    //check head slice buff
    if (buffer == NULL) {
        return -1;
    }
    if(phead[0] != 0x696c735b && phead[1] != 0x625f6563) {
        return -1;
    }
    //prase vi cnt
    stViCtx.u32WorkSnsCnt = ini_getl("vi_config", "sensor_cnt", 0, buffer);
    if (stViCtx.u32WorkSnsCnt == 0 ) {
#if (DUMP_DEBUG == 1)
        aos_debug_printf("parse ViParam error \r\n");
#endif
        return -1;
    }
    stViCtx.pstSensorCfg = (PARAM_SNS_CFG_S *)malloc(sizeof(PARAM_SNS_CFG_S) * stViCtx.u32WorkSnsCnt);
    stViCtx.pstDevInfo = (PARAM_DEV_CFG_S *)malloc(sizeof(PARAM_DEV_CFG_S) * stViCtx.u32WorkSnsCnt);
    stViCtx.pstChnInfo = (PARAM_CHN_CFG_S *)malloc(sizeof(PARAM_CHN_CFG_S) * stViCtx.u32WorkSnsCnt);
    stViCtx.pstIspCfg = (PARAM_ISP_CFG_S *)malloc(sizeof(PARAM_ISP_CFG_S) * stViCtx.u32WorkSnsCnt);
    if (stViCtx.pstSensorCfg) {
        memset(stViCtx.pstSensorCfg, 0 , sizeof(PARAM_SNS_CFG_S) * stViCtx.u32WorkSnsCnt);
    }
    if (stViCtx.pstDevInfo) {
        memset(stViCtx.pstDevInfo, 0 , sizeof(PARAM_DEV_CFG_S) * stViCtx.u32WorkSnsCnt);
    }
    if (stViCtx.pstChnInfo) {
        memset(stViCtx.pstChnInfo, 0 , sizeof(PARAM_CHN_CFG_S) * stViCtx.u32WorkSnsCnt);
    }
    if (stViCtx.pstIspCfg) {
        memset(stViCtx.pstIspCfg, 0 , sizeof(PARAM_ISP_CFG_S) * stViCtx.u32WorkSnsCnt);
    }
    //slice buffer mode
    int slice_buffer_mode = ini_getl("slice_buff", "slice_buff_mode", 0, buffer);
    for(unsigned int i = 0; i < stViCtx.u32WorkSnsCnt; i++) {
        //sensor param
        stViCtx.pstDevInfo[i].s8FbmMode = slice_buffer_mode == 0 ? 1 : 0;
        snprintf(_section, sizeof(_section), "sensor_config%d", i );
        stViCtx.pstSensorCfg[i].s32Framerate = (int)(ini_getl(_section, "framerate", 0, buffer));
        stViCtx.pstSensorCfg[i].MipiDev = ini_getl(_section, "mipi_dev", 0, buffer);
        stViCtx.pstSensorCfg[i].s32BusId = ini_getl(_section, "bus_id", 0, buffer);
        stViCtx.pstSensorCfg[i].s8I2cDev = stViCtx.pstSensorCfg[i].s32BusId;
        stViCtx.pstSensorCfg[i].s32I2cAddr = (int)ini_getl(_section, "sns_i2c_addr", 0, buffer);
        for (unsigned int j = 0; j < 5; j++) {
            snprintf(_key, sizeof(_key), "laneid%d", j);
            stViCtx.pstSensorCfg[i].as16LaneId[j] = (short int)ini_getl(_section, _key, -1, buffer);
            snprintf(_key, sizeof(_key), "swap%d", j);
            stViCtx.pstSensorCfg[i].as8PNSwap[j] = (char)ini_getl(_section, _key, -1, buffer);
        }
        stViCtx.pstSensorCfg[i].bMclkEn = (short int)ini_getl(_section, "mclk_en", -1, buffer);
        if (stViCtx.pstSensorCfg[i].bMclkEn == CVI_TRUE) {
            memset(_stringPrase, 0, sizeof(_stringPrase));
            ini_gets(_section, "mclk", "", _stringPrase, sizeof(_stringPrase), buffer);
            unsigned int _mclk = stringHash(_stringPrase);
            for (unsigned int j = 0 ; j < sizeof(as32Mclk)/sizeof(as32Mclk[0]); j++) {
                if (_mclk == as32Mclk[j]) {
                    stViCtx.pstSensorCfg[i].u8Mclk = j;
                    break;
                }
            }
            memset(_stringPrase, 0, sizeof(_stringPrase));
            ini_gets(_section, "mclk_freq", "", _stringPrase, sizeof(_stringPrase), buffer);
            unsigned int _mclk_freq = stringHash(_stringPrase);
            for (unsigned int j = 0 ; j < sizeof(as32MclkFreq)/sizeof(as32MclkFreq[0]); j++) {
                if (_mclk_freq == as32MclkFreq[j]) {
                    stViCtx.pstSensorCfg[i].u8MclkFreq = j;
                    break;
                }
            }
            stViCtx.pstSensorCfg[i].u8MclkCam = (short int)ini_getl(_section, "mclk_cam", -1, buffer);
        }
        stViCtx.pstSensorCfg[i].u8Orien = (short int)ini_getl(_section, "orien", -1, buffer);
        stViCtx.pstSensorCfg[i].bHwSync = (short int)ini_getl(_section, "hw_sync", -1, buffer);
        stViCtx.pstSensorCfg[i].u32Rst_port_idx = (short int)ini_getl(_section, "rst_port_idx", -1, buffer);
        stViCtx.pstSensorCfg[i].u32Rst_pol = (short int)ini_getl(_section, "rst_pol", -1, buffer);
        stViCtx.pstSensorCfg[i].u32Rst_pin = (short int)ini_getl(_section, "rst_pin", -1, buffer);
        stViCtx.pstSensorCfg[i].u8Rotation = (short int)ini_getl(_section, "rotation", 0, buffer);
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "sns_type", "", _stringPrase, sizeof(_stringPrase), buffer);
        unsigned int _sns_type = stringHash(_stringPrase);
        for (unsigned int j = 0; j < sizeof(as32SensorType)/sizeof(as32SensorType[0]); j++) {
            if (_sns_type == as32SensorType[j]) {
                stViCtx.pstSensorCfg[i].enSnsType = j;
                set_sensor_type(j, i);
                break;
            }
        }
        //vi param
        snprintf(_section, sizeof(_section), "vi_cfg_dev%d", i);
        stViCtx.pstChnInfo[i].s32ChnId =  (int)ini_getl(_section, "videv", -1, buffer);
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "wdrmode", "", _stringPrase, sizeof(_stringPrase), buffer);
        int _wdrmode = stringHash(_stringPrase);
        for (unsigned int j = 0; j < sizeof(as32ViWdrMode)/sizeof(as32ViWdrMode[0]); j++) {
            if (_wdrmode == as32ViWdrMode[j]) {
                stViCtx.pstChnInfo[i].enWDRMode = j;
                break;
            }
        }
        snprintf(_section, sizeof(_section), "vi_cfg_chn%d", i);
        stViCtx.pstChnInfo[i].enDynamicRange = DYNAMIC_RANGE_SDR8;//default
        stViCtx.pstChnInfo[i].bYuvBypassPath = 0;//yuv sensor use
        stViCtx.pstChnInfo[i].f32Fps = (int)ini_getl(_section, "fps", -1, buffer);
        stViCtx.pstChnInfo[i].u32Width = (int)ini_getl(_section, "width", 0, buffer);
        stViCtx.pstChnInfo[i].u32Height = (int)ini_getl(_section, "height", 0, buffer);
        stViCtx.pstChnInfo[i].enVideoFormat = VIDEO_FORMAT_LINEAR;
        memset(_stringPrase, 0 , sizeof(_stringPrase));
        ini_gets(_section, "compress_mode", "", _stringPrase, sizeof(_stringPrase), buffer);
        int _compress_mode = stringHash(_stringPrase);
        for (unsigned int j = 0; j < sizeof(as32CompressMode)/sizeof(as32CompressMode[0]); j++) {
            if (_compress_mode == as32CompressMode[j]) {
                stViCtx.pstChnInfo[i].enCompressMode = j;
                break;
            }
        }
        //
        stViCtx.pstIspCfg[i].s8ByPassNum = ini_getl(_section, "bypassnum", 5 , buffer);
        //fastConverge Prase
        stViCtx.bFastConverge = (bool)ini_getl("fastConverge", "enable", 0, buffer);
        if (stViCtx.bFastConverge == CVI_TRUE) {
            stViCtx.pstIspCfg[i].s8FastConvergeAvailableNode = ini_getl("fastConverge", "availableNode", 0, buffer);
            for (int j = 0; j < stViCtx.pstIspCfg[i].s8FastConvergeAvailableNode; j++) {
                snprintf(_key, sizeof(_key), "firstfFrLuma%d", j);
                stViCtx.pstIspCfg[i].as16firstFrLuma[j] = ini_getl("fastConverge", _key, 0, buffer);
                snprintf(_key, sizeof(_key), "targetBv%d", j);
                stViCtx.pstIspCfg[i].as16targetBv[j] = ini_getl("fastConverge", _key, 0, buffer);
            }
        }
    }
#if (DUMP_DEBUG == 1)
    aos_debug_printf("stViCtx.pstDevInfo->s8FbmMode is %d \r\n", stViCtx.pstDevInfo->s8FbmMode);
    aos_debug_printf("stViCtx.u32WorkSnsCnt is %d \r\n", stViCtx.u32WorkSnsCnt);
    aos_debug_printf("stViCtx.u32IspSceneNum is %d \r\n", stViCtx.u32IspSceneNum);
    aos_debug_printf("stViCtx.bFastConverge is %d \r\n", stViCtx.bFastConverge);
    for (int i = 0; i < stViCtx.u32WorkSnsCnt; i++) {
        aos_debug_printf("stViCtx.pstSensorCfg[%d].s32Framerate %d \r\n", i,stViCtx.pstSensorCfg[i].s32Framerate);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].enWDRMode %d \r\n", i,stViCtx.pstSensorCfg[i].enWDRMode);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].s8I2cDev %d \r\n", i,stViCtx.pstSensorCfg[i].s8I2cDev);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].s32BusId %d \r\n", i,stViCtx.pstSensorCfg[i].s32BusId);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].s32I2cAddr %d \r\n", i,stViCtx.pstSensorCfg[i].s32I2cAddr);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].MipiDev %d \r\n", i,stViCtx.pstSensorCfg[i].MipiDev);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].bMclkEn %d \r\n", i,stViCtx.pstSensorCfg[i].bMclkEn);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u8Mclk %d \r\n", i,stViCtx.pstSensorCfg[i].u8Mclk);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u8MclkCam %d \r\n", i,stViCtx.pstSensorCfg[i].u8MclkCam);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u8MclkFreq %d \r\n", i,stViCtx.pstSensorCfg[i].u8MclkFreq);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u8Orien %d \r\n", i,stViCtx.pstSensorCfg[i].u8Orien);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].bHwSync %d \r\n", i,stViCtx.pstSensorCfg[i].bHwSync);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u32Rst_port_idx %d \r\n", i,stViCtx.pstSensorCfg[i].u32Rst_port_idx);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u32Rst_pin %d \r\n", i,stViCtx.pstSensorCfg[i].u32Rst_pin);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u32Rst_pol %d \r\n", i,stViCtx.pstSensorCfg[i].u32Rst_pol);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].u8Rotation %d \r\n", i,stViCtx.pstSensorCfg[i].u8Rotation);
        aos_debug_printf("stViCtx.pstSensorCfg[%d].enSnsType %d \r\n", i,stViCtx.pstSensorCfg[i].enSnsType);


        //vi param
        aos_debug_printf("stViCtx.pstChnInfo[%d].s32ChnId %d \r\n", i,stViCtx.pstChnInfo[i].s32ChnId);
        aos_debug_printf("stViCtx.pstChnInfo[%d].enWDRMode %d \r\n", i,stViCtx.pstChnInfo[i].enWDRMode);
        aos_debug_printf("stViCtx.pstChnInfo[%d].enDynamicRange %d \r\n", i,stViCtx.pstChnInfo[i].enDynamicRange);
        aos_debug_printf("stViCtx.pstChnInfo[%d].bYuvBypassPath %d \r\n", i,stViCtx.pstChnInfo[i].bYuvBypassPath);
        aos_debug_printf("stViCtx.pstChnInfo[%d].f32Fps %d \r\n", i,(int)stViCtx.pstChnInfo[i].f32Fps);
        aos_debug_printf("stViCtx.pstChnInfo[%d].u32Width %d \r\n", i,stViCtx.pstChnInfo[i].u32Width);
        aos_debug_printf("stViCtx.pstChnInfo[%d].u32Height %d \r\n", i,stViCtx.pstChnInfo[i].u32Height);
        aos_debug_printf("stViCtx.pstChnInfo[%d].enVideoFormat %d \r\n", i,stViCtx.pstChnInfo[i].enVideoFormat);
        aos_debug_printf("stViCtx.pstChnInfo[%d].enCompressMode %d \r\n", i,stViCtx.pstChnInfo[i].enCompressMode);


        aos_debug_printf("stViCtx.pstIspCfg[%d].s8FastConvergeAvailableNode %d \r\n", i,stViCtx.pstIspCfg[i].s8FastConvergeAvailableNode);
        for (int j = 0; j < stViCtx.pstIspCfg[i].s8FastConvergeAvailableNode; j++) {
                aos_debug_printf("stViCtx.pstIspCfg[%d].as16firstFrLuma[%d] %d\r\n", i,j, stViCtx.pstIspCfg[i].as16firstFrLuma[j]);
                aos_debug_printf("stViCtx.pstIspCfg[%d].as16targetBv[%d] %d\r\n", i,j, stViCtx.pstIspCfg[i].as16targetBv[j]);
        }
    }
#endif
    return 0;
}

int iniParseHashMapInit()
{
    //sensor type map
    as32SensorType[SONY_IMX327_MIPI_2M_30FPS_12BIT] = stringHash("SONY_IMX327_MIPI_2M_30FPS_12BIT");
    as32SensorType[SONY_IMX307_MIPI_2M_30FPS_12BIT] = stringHash("SONY_IMX307_MIPI_2M_30FPS_12BIT");
    as32SensorType[SONY_IMX307_SLAVE_MIPI_2M_30FPS_12BIT] = stringHash("SONY_IMX307_SLAVE_MIPI_2M_30FPS_12BIT");
    as32SensorType[GCORE_GC1054_MIPI_1M_30FPS_10BIT] = stringHash("GCORE_GC1054_MIPI_1M_30FPS_10BIT");
    as32SensorType[GCORE_GC2053_MIPI_2M_30FPS_10BIT] = stringHash("GCORE_GC2053_MIPI_2M_30FPS_10BIT");
    as32SensorType[GCORE_GC2093_MIPI_2M_30FPS_10BIT] = stringHash("GCORE_GC2093_MIPI_2M_30FPS_10BIT");
    as32SensorType[GCORE_GC02M1_MIPI_2M_30FPS_10BIT] = stringHash("GCORE_GC02M1_MIPI_2M_30FPS_10BIT");
    as32SensorType[GCORE_GC02M1_SLAVE_MIPI_2M_30FPS_10BIT] = stringHash("GCORE_GC02M1_SLAVE_MIPI_2M_30FPS_10BIT");
    as32SensorType[GCORE_GC4653_MIPI_4M_30FPS_10BIT] = stringHash("GCORE_GC4653_MIPI_4M_30FPS_10BIT");
    as32SensorType[GCORE_GC2053_1L_MIPI_2M_30FPS_10BIT] = stringHash("GCORE_GC2053_1L_MIPI_2M_30FPS_10BIT");
    as32SensorType[SMS_SC2331_1L_MIPI_2M_30FPS_10BIT] = stringHash("SMS_SC2331_1L_MIPI_2M_30FPS_10BIT");
    as32SensorType[SMS_SC035HGS_1L_MIPI_480P_120FPS_10BIT] = stringHash("SMS_SC035HGS_1L_MIPI_480P_120FPS_10BIT");
    as32SensorType[SMS_SC035HGS_1L_SLAVE_MIPI_480P_120FPS_10BIT] = stringHash("SMS_SC035HGS_1L_SLAVE_MIPI_480P_120FPS_10BIT");
    //vpss mode map
    as32VpssAenInput[VPSS_INPUT_MEM] = stringHash("VPSS_INPUT_MEM");
    as32VpssAenInput[VPSS_INPUT_ISP] = stringHash("VPSS_INPUT_ISP");
    as32VpssMode[VPSS_MODE_SINGLE] = stringHash("VPSS_MODE_SINGLE");
    as32VpssMode[VPSS_MODE_DUAL] = stringHash("VPSS_MODE_DUAL");
    as32VpssMode[VPSS_MODE_RGNEX] = stringHash("VPSS_MODE_RGNEX");
    //vi-vpss mode map
    as32ViVpssMode[VI_OFFLINE_VPSS_OFFLINE] = stringHash("VI_OFFLINE_VPSS_OFFLINE");
    as32ViVpssMode[VI_OFFLINE_VPSS_ONLINE] = stringHash("VI_OFFLINE_VPSS_ONLINE");
    as32ViVpssMode[VI_ONLINE_VPSS_OFFLINE] = stringHash("VI_ONLINE_VPSS_OFFLINE");
    as32ViVpssMode[VI_ONLINE_VPSS_ONLINE] = stringHash("VI_ONLINE_VPSS_ONLINE");
    //PixFormat map
    as32PixFormat[PIXEL_FORMAT_NV21] = stringHash("PIXEL_FORMAT_NV21");
    as32PixFormat[PIXEL_FORMAT_NV12] = stringHash("PIXEL_FORMAT_NV12");
    as32PixFormat[PIXEL_FORMAT_RGB_888] = stringHash("PIXEL_FORMAT_RGB_888");
    as32PixFormat[PIXEL_FORMAT_BGR_888] = stringHash("PIXEL_FORMAT_BGR_888");
    as32PixFormat[PIXEL_FORMAT_RGB_888_PLANAR] = stringHash("PIXEL_FORMAT_RGB_888_PLANAR");
    as32PixFormat[PIXEL_FORMAT_BGR_888_PLANAR] = stringHash("PIXEL_FORMAT_BGR_888_PLANAR");
    as32PixFormat[PIXEL_FORMAT_ARGB_1555] = stringHash("PIXEL_FORMAT_ARGB_1555");
    as32PixFormat[PIXEL_FORMAT_ARGB_4444] = stringHash("PIXEL_FORMAT_ARGB_4444");
    as32PixFormat[PIXEL_FORMAT_ARGB_8888] = stringHash("PIXEL_FORMAT_ARGB_8888");
    as32PixFormat[PIXEL_FORMAT_YUV_400] = stringHash("PIXEL_FORMAT_YUV_400");
    as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_444] = stringHash("PIXEL_FORMAT_YUV_PLANAR_444");
    as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_420] = stringHash("PIXEL_FORMAT_YUV_PLANAR_420");
    as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_422] = stringHash("PIXEL_FORMAT_YUV_PLANAR_422");

    as32ViWdrMode[WDR_MODE_NONE] = stringHash("WDR_MODE_NONE");
    as32ViWdrMode[WDR_MODE_BUILT_IN] = stringHash("WDR_MODE_BUILT_IN");
    as32ViWdrMode[WDR_MODE_QUDRA] = stringHash("WDR_MODE_QUDRA");
    as32ViWdrMode[WDR_MODE_2To1_LINE] = stringHash("WDR_MODE_2To1_LINE");
    as32ViWdrMode[WDR_MODE_2To1_FRAME] = stringHash("WDR_MODE_2To1_FRAME");
    as32ViWdrMode[WDR_MODE_2To1_FRAME_FULL_RATE] = stringHash("WDR_MODE_2To1_FRAME_FULL_RATE");
    as32ViWdrMode[WDR_MODE_3To1_LINE] = stringHash("WDR_MODE_3To1_LINE");
    as32ViWdrMode[WDR_MODE_3To1_FRAME] = stringHash("WDR_MODE_3To1_FRAME");
    as32ViWdrMode[WDR_MODE_3To1_FRAME_FULL_RATE] = stringHash("WDR_MODE_3To1_FRAME_FULL_RATE");
    as32ViWdrMode[WDR_MODE_4To1_LINE] = stringHash("WDR_MODE_4To1_LINE");
    as32ViWdrMode[WDR_MODE_4To1_FRAME] = stringHash("WDR_MODE_4To1_FRAME");
    as32ViWdrMode[WDR_MODE_4To1_FRAME_FULL_RATE] = stringHash("WDR_MODE_4To1_FRAME_FULL_RATE");

    as32CompressMode[COMPRESS_MODE_NONE] = stringHash("COMPRESS_MODE_NONE");
    as32CompressMode[COMPRESS_MODE_TILE] = stringHash("COMPRESS_MODE_TILE");
    as32CompressMode[COMPRESS_MODE_LINE] = stringHash("COMPRESS_MODE_LINE");
    as32CompressMode[COMPRESS_MODE_FRAME] = stringHash("COMPRESS_MODE_FRAME");

    as32Mclk[RX_MAC_CLK_200M] = stringHash("RX_MAC_CLK_200M");
    as32Mclk[RX_MAC_CLK_300M] = stringHash("RX_MAC_CLK_300M");
    as32Mclk[RX_MAC_CLK_400M] = stringHash("RX_MAC_CLK_400M");
    as32Mclk[RX_MAC_CLK_500M] = stringHash("RX_MAC_CLK_500M");
    as32Mclk[RX_MAC_CLK_600M] = stringHash("RX_MAC_CLK_600M");

    as32MclkFreq[CAMPLL_FREQ_NONE] = stringHash("CAMPLL_FREQ_NONE");
    as32MclkFreq[CAMPLL_FREQ_37P125M] = stringHash("CAMPLL_FREQ_37P125M");
    as32MclkFreq[CAMPLL_FREQ_25M] = stringHash("CAMPLL_FREQ_25M");
    as32MclkFreq[CAMPLL_FREQ_27M] = stringHash("CAMPLL_FREQ_27M");
    as32MclkFreq[CAMPLL_FREQ_24M] = stringHash("CAMPLL_FREQ_24M");
    as32MclkFreq[CAMPLL_FREQ_26M] = stringHash("CAMPLL_FREQ_26M");


#if (DUMP_DEBUG == 1)
    aos_debug_printf("The GCORE_GC2053_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC2053_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The SONY_IMX327_MIPI_2M_30FPS_12BIT %d \r\n", as32SensorType[SONY_IMX327_MIPI_2M_30FPS_12BIT]);
    aos_debug_printf("The SONY_IMX307_MIPI_2M_30FPS_12BIT %d \r\n", as32SensorType[SONY_IMX307_MIPI_2M_30FPS_12BIT]);
    aos_debug_printf("The SONY_IMX307_SLAVE_MIPI_2M_30FPS_12BIT %d \r\n", as32SensorType[SONY_IMX307_SLAVE_MIPI_2M_30FPS_12BIT]);
    aos_debug_printf("The GCORE_GC1054_MIPI_1M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC1054_MIPI_1M_30FPS_10BIT]);
    aos_debug_printf("The GCORE_GC2093_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC2093_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The GCORE_GC02M1_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC02M1_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The GCORE_GC02M1_SLAVE_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC02M1_SLAVE_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The GCORE_GC4653_MIPI_4M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC4653_MIPI_4M_30FPS_10BIT]);
    aos_debug_printf("The GCORE_GC2053_1L_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[GCORE_GC2053_1L_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The SMS_SC2331_1L_MIPI_2M_30FPS_10BIT %d \r\n", as32SensorType[SMS_SC2331_1L_MIPI_2M_30FPS_10BIT]);
    aos_debug_printf("The SMS_SC035HGS_1L_MIPI_480P_120FPS_10BIT %d \r\n", as32SensorType[SMS_SC035HGS_1L_MIPI_480P_120FPS_10BIT]);
    aos_debug_printf("The SMS_SC035HGS_1L_SLAVE_MIPI_480P_120FPS_10BIT %d \r\n", as32SensorType[SMS_SC035HGS_1L_SLAVE_MIPI_480P_120FPS_10BIT]);

    aos_debug_printf("The as32VpssAenInput[VPSS_INPUT_MEM] %d \r\n", as32VpssAenInput[VPSS_INPUT_MEM]);
    aos_debug_printf("The as32VpssAenInput[VPSS_INPUT_ISP] %d \r\n", as32VpssAenInput[VPSS_INPUT_ISP]);
    aos_debug_printf("The VPSS_MODE_SINGLE %d \r\n", as32VpssMode[VPSS_MODE_SINGLE]);
    aos_debug_printf("The VPSS_MODE_DUAL %d \r\n", as32VpssMode[VPSS_MODE_DUAL]);
    aos_debug_printf("The VI_OFFLINE_VPSS_OFFLINE %d \r\n", as32ViVpssMode[VI_OFFLINE_VPSS_OFFLINE]);
    aos_debug_printf("The VI_OFFLINE_VPSS_ONLINE %d \r\n", as32ViVpssMode[VI_OFFLINE_VPSS_ONLINE]);
    aos_debug_printf("The VI_ONLINE_VPSS_OFFLINE %d \r\n", as32ViVpssMode[VI_ONLINE_VPSS_OFFLINE]);
    aos_debug_printf("The VI_ONLINE_VPSS_ONLINE %d \r\n", as32ViVpssMode[VI_ONLINE_VPSS_ONLINE]);

    aos_debug_printf("The PIXEL_FORMAT_NV21 %d \r\n", as32PixFormat[PIXEL_FORMAT_NV21]);
    aos_debug_printf("The PIXEL_FORMAT_NV12 %d \r\n", as32PixFormat[PIXEL_FORMAT_NV12]);
    aos_debug_printf("The PIXEL_FORMAT_RGB_888 %d \r\n", as32PixFormat[PIXEL_FORMAT_RGB_888]);
    aos_debug_printf("The PIXEL_FORMAT_BGR_888 %d \r\n", as32PixFormat[PIXEL_FORMAT_BGR_888]);
    aos_debug_printf("The PIXEL_FORMAT_RGB_888_PLANAR %d \r\n", as32PixFormat[PIXEL_FORMAT_RGB_888_PLANAR]);
    aos_debug_printf("The PIXEL_FORMAT_BGR_888_PLANAR %d \r\n", as32PixFormat[PIXEL_FORMAT_BGR_888_PLANAR]);
    aos_debug_printf("The PIXEL_FORMAT_ARGB_1555 %d \r\n", as32PixFormat[PIXEL_FORMAT_ARGB_1555]);
    aos_debug_printf("The PIXEL_FORMAT_ARGB_4444 %d \r\n", as32PixFormat[PIXEL_FORMAT_ARGB_4444]);
    aos_debug_printf("The PIXEL_FORMAT_ARGB_8888 %d \r\n", as32PixFormat[PIXEL_FORMAT_ARGB_8888]);
    aos_debug_printf("The PIXEL_FORMAT_YUV_400 %d \r\n", as32PixFormat[PIXEL_FORMAT_YUV_400]);
    aos_debug_printf("The PIXEL_FORMAT_YUV_PLANAR_444 %d \r\n", as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_444]);
    aos_debug_printf("The PIXEL_FORMAT_YUV_PLANAR_420 %d \r\n", as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_420]);
    aos_debug_printf("The PIXEL_FORMAT_YUV_PLANAR_422 %d \r\n", as32PixFormat[PIXEL_FORMAT_YUV_PLANAR_422]);

    aos_debug_printf("The COMPRESS_MODE_NONE %d \r\n", as32CompressMode[COMPRESS_MODE_NONE]);
    aos_debug_printf("The COMPRESS_MODE_TILE %d \r\n", as32CompressMode[COMPRESS_MODE_TILE]);
    aos_debug_printf("The COMPRESS_MODE_LINE %d \r\n", as32CompressMode[COMPRESS_MODE_LINE]);
    aos_debug_printf("The COMPRESS_MODE_FRAME %d \r\n", as32CompressMode[COMPRESS_MODE_FRAME]);

    aos_debug_printf("The WDR_MODE_NONE %d \r\n", as32ViWdrMode[WDR_MODE_NONE]);
    aos_debug_printf("The WDR_MODE_BUILT_IN %d \r\n", as32ViWdrMode[WDR_MODE_BUILT_IN]);
    aos_debug_printf("The WDR_MODE_QUDRA %d \r\n", as32ViWdrMode[WDR_MODE_QUDRA]);
    aos_debug_printf("The WDR_MODE_2To1_LINE %d \r\n", as32ViWdrMode[WDR_MODE_2To1_LINE]);
    aos_debug_printf("The WDR_MODE_2To1_FRAME %d \r\n", as32ViWdrMode[WDR_MODE_2To1_FRAME]);
    aos_debug_printf("The WDR_MODE_2To1_FRAME_FULL_RATE %d \r\n", as32ViWdrMode[WDR_MODE_2To1_FRAME_FULL_RATE]);
    aos_debug_printf("The WDR_MODE_3To1_LINE %d \r\n", as32ViWdrMode[WDR_MODE_3To1_LINE]);
    aos_debug_printf("The WDR_MODE_3To1_FRAME %d \r\n", as32ViWdrMode[WDR_MODE_3To1_FRAME]);
    aos_debug_printf("The WDR_MODE_3To1_FRAME_FULL_RATE %d \r\n", as32ViWdrMode[WDR_MODE_3To1_FRAME_FULL_RATE]);
    aos_debug_printf("The WDR_MODE_4To1_LINE %d \r\n", as32ViWdrMode[WDR_MODE_4To1_LINE]);
    aos_debug_printf("The WDR_MODE_4To1_FRAME %d \r\n", as32ViWdrMode[WDR_MODE_4To1_FRAME]);
    aos_debug_printf("The WDR_MODE_4To1_FRAME_FULL_RATE %d \r\n", as32ViWdrMode[WDR_MODE_4To1_FRAME_FULL_RATE]);

    aos_debug_printf("The RX_MAC_CLK_200M %d \r\n", as32Mclk[RX_MAC_CLK_200M]);
    aos_debug_printf("The RX_MAC_CLK_300M %d \r\n", as32Mclk[RX_MAC_CLK_300M]);
    aos_debug_printf("The RX_MAC_CLK_400M %d \r\n", as32Mclk[RX_MAC_CLK_400M]);
    aos_debug_printf("The RX_MAC_CLK_500M %d \r\n", as32Mclk[RX_MAC_CLK_500M]);
    aos_debug_printf("The RX_MAC_CLK_600M %d \r\n", as32Mclk[RX_MAC_CLK_600M]);

    aos_debug_printf("The CAMPLL_FREQ_NONE %d \r\n", as32MclkFreq[CAMPLL_FREQ_NONE]);
    aos_debug_printf("The CAMPLL_FREQ_37P125M %d \r\n", as32MclkFreq[CAMPLL_FREQ_37P125M]);
    aos_debug_printf("The CAMPLL_FREQ_25M %d \r\n", as32MclkFreq[CAMPLL_FREQ_25M]);
    aos_debug_printf("The CAMPLL_FREQ_27M %d \r\n", as32MclkFreq[CAMPLL_FREQ_27M]);
    aos_debug_printf("The CAMPLL_FREQ_24M %d \r\n", as32MclkFreq[CAMPLL_FREQ_24M]);
    aos_debug_printf("The CAMPLL_FREQ_26M %d \r\n", as32MclkFreq[CAMPLL_FREQ_26M]);

#endif
    return 0;
}

static int APP_Param_PqParmParse()
{
    #if (DUMP_DEBUG == 1)
    aos_debug_printf("CVI_IPCM_GetPQBinQddr() 0x%x\r\n", CVI_IPCM_GetPQBinQddr());
    #endif
    PARTITION_CHECK_HAED_S * pstPqParam = (PARTITION_CHECK_HAED_S *)((unsigned long)CVI_IPCM_GetPQBinQddr());
    if (_param_check_head((unsigned char *)pstPqParam) != 0) {
        return -1;
    }
    unsigned char * pPqBuffer = (unsigned char *)pstPqParam + sizeof(PARTITION_CHECK_HAED_S);
    for (unsigned int i = 0; i < pstPqParam->package_number; i++) {
        stViCtx.pstIspCfg[i / 2].stPQBinDes[i % 2].pIspBinData = pPqBuffer;
        stViCtx.pstIspCfg[i / 2].stPQBinDes[i % 2].u32IspBinDataLen = pstPqParam->package_length[i];
        #if (DUMP_DEBUG == 1)
        aos_debug_printf("pPqBuffer %p\r\n", pPqBuffer);
        aos_debug_printf("stViCtx.pstIspCfg[%d].stPQBinDes[%d].pIspBinData %x\r\n", i/2, i %2, *pPqBuffer);
        aos_debug_printf("stViCtx.pstIspCfg[%d].stPQBinDes[%d].u32IspBinDataLen %d\r\n", i/2, i %2, stViCtx.pstIspCfg[i / 2].stPQBinDes[i % 2].u32IspBinDataLen);
        #endif
        pPqBuffer += pstPqParam->package_length[i];
    }

    return 0;
}

static void _param_init()
{
    PARAM_MANAGER_CFG_S * pstParam = PARAM_GET_MANAGER_CFG();
    pstParam->pstSysCtx = &stSysCtx;
    pstParam->pstViCtx = &stViCtx;
    pstParam->pstVpssCfg = NULL;
    pstParam->pstVencCfg = NULL;
    pstParam->pstVoCfg = NULL;
}

int APP_PARAM_Parse()
{
    _param_init();
    //VB Parse
    #if (DUMP_DEBUG == 1)
    aos_debug_printf("CVI_IPCM_GetParamBinAddr() 0x%x\r\n", CVI_IPCM_GetParamBinAddr());
    #endif
    iniParseHashMapInit();
    char * ini_string = (char *)((unsigned long)CVI_IPCM_GetParamBinAddr());
    if (_param_check_head((unsigned char *)ini_string) != 0) {
        aos_debug_printf("%s err \r\n", __func__);
        return -1;
    }
    ini_string += sizeof(PARTITION_CHECK_HAED_S);
    parse_SysParam(ini_string);
    parse_ViParam(ini_string);
    APP_Param_PqParmParse();
    return 0;
}

