
#include <unistd.h>
#include <aos/kernel.h>
#include "ipcm_test_common_anon.h"

static int _anon_test_running = 0;

static void ipcm_anon_test_task(void *paras)
{
    sleep(1);
    // test anon api
    anon_test_send_msg();
    anon_test_send_param();
    sleep(1);
    // test CVI_IPCM ANON api
    ipcm_anon_test_cvi();
    sleep(1);
    // restore ipcm anon handle
    anon_test_uninit();
    anon_test_init();

    _anon_test_running = 0;
}

static s32 _anon_test_handle_hook(void *priv, ipcm_anon_msg_t *data)
{
    if (_anon_test_running == 0) {
        _anon_test_running = 1;
        aos_task_new("ipcm anon test task", ipcm_anon_test_task, 0, 4*1024);
    }

    return 0;
}

s32 ipcm_anon_test_main(void)
{
    anon_test_init();
    anon_test_register_handle_hook(_anon_test_handle_hook, NULL);

    return 0;
}
