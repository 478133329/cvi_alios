
#include <aos/cli.h>
#include <string.h>
#include <stdio.h>
#include <cvi_vip.h>
#include "mailbox.h"
#include "ipcm_common.h"

extern int ipcm_print_pool_proc(void);
extern void ipcm_msg_set_rs_log_stat(u8 stat);

static void ipcm_dbg_print_help(void)
{
    ipcm_info("ipcm_dbg help:\r\n");
    ipcm_info("ipcm_dbg info : show ipcm info.\r\n");
    ipcm_info("ipcm_dbg pool_info : show pool get/rls callstack.\r\n");
    ipcm_info("ipcm_dbg log0_on  : open ipcm recv and send log.\r\n");
    ipcm_info("ipcm_dbg log0_off : close ipcm recv and send log.\r\n");
    ipcm_info("ipcm_dbg logall_on  : open all log(set log level to debug).\r\n");
    ipcm_info("ipcm_dbg logall_off : reset log level to default.\r\n");
}

static int ipcm_info_show(void)
{
    ipcm_info("mailbox not valid cnt:%d\n", mailbox_get_invalid_cnt());
    return 0;
}

static int ipcm_pool_info_show(void)
{
    ipcm_print_pool_proc();
    return 0;
}

static void ipcm_proc_debug(int32_t argc, char **argv)
{
    if (argc < 2) {
        ipcm_dbg_print_help();
        return;
    }

    if (0 == strcmp(argv[1], "info")) {
	    ipcm_info_show();
    }else if (0 == strcmp(argv[1], "pool_info")) {
	    ipcm_pool_info_show();
    } else if (0 == strcmp(argv[1], "log0_on")) {
        ipcm_msg_set_rs_log_stat(1);
    } else if (0 == strcmp(argv[1], "log0_off")) {
        ipcm_msg_set_rs_log_stat(0);
    } else if (0 == strcmp(argv[1], "logall_on")) {
        ipcm_set_log_level(IPCM_LOG_DEBUG);
    } else if (0 == strcmp(argv[1], "logall_off")) {
        ipcm_set_log_level(IPCM_LOG_LEVEL_DEFAULT);
    } else {
        ipcm_dbg_print_help();
    }
}

ALIOS_CLI_CMD_REGISTER(ipcm_proc_debug, ipcm_dbg, ipcm debug);
