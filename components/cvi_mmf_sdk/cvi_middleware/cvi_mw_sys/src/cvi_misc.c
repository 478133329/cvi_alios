#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <queue.h>
#include <pthread.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <aos/aos.h>
#include <unistd.h>

#include "cvi_mw_base.h"
#include "cvi_sys.h"
#include "cvi_errno.h"
#include "sys_ioctl.h"
#include "cvi_efuse.h"
#include "aos/cli.h"

#include <cvi_base.h>

#define CVI_EFUSE_CUSTOMER_ADDR 0x1

CVI_S32 CVI_EFUSE_EnableFastBoot(void)
{
	csi_error_t ret = CSI_OK;
	CVI_U32 value = 0;
	CVI_U32 chip = 0;

	ret = cvi_efuse_pwr_on();
	if (ret != CSI_OK) {
		printf("efuse power on failed\n");
		return CVI_FAILURE;
	}

	ret = cvi_efuse_disable_uart_dl();
	ret |= cvi_efuse_set_sd_dl_button();
	if (ret != CSI_OK) {
		printf("disable uart dl or set sd dl button failed\n");
		cvi_efuse_pwr_off();
		return CVI_FAILURE;
	}

	CVI_SYS_GetChipId(&chip);
	if (IS_CHIP_MARS(chip)) {
		if (IS_CHIP_PKG_TYPE_QFN(chip))
			value = 0x1E1E64; // AUX0
		else
			value = 0x1; // USB_ID

		ret = cvi_efuse_program_word(CVI_EFUSE_CUSTOMER_ADDR, value);
		if (ret != CSI_OK) {
			printf("efuse program customer failed\n");
			cvi_efuse_pwr_off();
			return CVI_FAILURE;
		}
	} else {
		CVI_TRACE_SYS(CVI_DBG_DEBUG, "IS_CHIP_MARS=%d\n", IS_CHIP_MARS(chip));
		CVI_TRACE_SYS(CVI_DBG_DEBUG, "IS_CHIP_PKG_TYPE_QFN=%d\n", IS_CHIP_PKG_TYPE_QFN(chip));
	}

	cvi_efuse_pwr_off();
	return CVI_SUCCESS;
}

CVI_S32 CVI_EFUSE_IsFastBootEnabled(void)
{
    csi_error_t ret = CSI_OK;
    CVI_U32 value = 0;
	CVI_U32 chip = 0;

    ret = cvi_efuse_is_uart_dl_enable();
    ret |= cvi_efuse_is_sd_dl_button();
    if (ret != CSI_OK) {
        return CVI_FAILURE;
    }

	ret = cvi_efuse_read_word_from_shadow(CVI_EFUSE_CUSTOMER_ADDR, &value);
	CVI_TRACE_SYS(CVI_DBG_DEBUG, "ret=%d value=%u\n", ret, value);
	if (ret < 0)
		return ret;

	CVI_SYS_GetChipId(&chip);
	if (IS_CHIP_MARS(chip) && IS_CHIP_PKG_TYPE_QFN(chip)) {
		if (value == 0x1E1E64)
			return CVI_SUCCESS; // AUX0
		else if (value == 0x1)
			return CVI_SUCCESS; // USB_ID
		else
			return CVI_FAILURE;
	} else
		return CVI_FAILURE;
}

#if (CONFIG_EFUSE_TEST == 1)

void cvi_efuse_program(int32_t argc, char **argv)
{
	uint32_t addr, data;
	char *ptr;
	long value = -1;
	csi_error_t ret = CSI_OK;

	if (argc != 3) {
		printf("invailed param! \n usage: efusew addr data\n");
		return;
	}

	value = strtol(argv[1], &ptr, 16);
	if (value < 0) {
		printf("invailed addr! \n");
		return;
	}

	addr = (uint32_t)value;
	data = strtol(argv[2], &ptr, 16);

	ret = cvi_efuse_pwr_on();
    if (ret != CSI_OK) {
        printf("cvi_efuse_pwr_on failed\n");
        return;
    }

	ret = cvi_efuse_program_word(addr, data);
	if (ret != CSI_OK) {
        printf("cvi_efuse_program_word failed\n");
    }

    cvi_efuse_pwr_off();
}

ALIOS_CLI_CMD_REGISTER(cvi_efuse_program, efusew, efuse write);

#endif
