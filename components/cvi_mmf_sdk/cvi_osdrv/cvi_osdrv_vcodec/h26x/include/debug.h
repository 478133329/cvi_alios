//--=========================================================================--
//  This file is a part of VPU Reference API project
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--
#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "config.h"
#include "main_helper.h"

enum {
	CNMQC_ENV_NONE,
	CNMQC_ENV_GDBSERVER = (1 << 16), /*!<< It executes gdb server in order to debug F/W on the C&M FPGA env. */
	CNMQC_ENV_MAX,
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef ENABLE_CNM_DEBUG_MSG
void PrintVpuStatus(Uint32 coreIdx, Uint32 productId);
#endif

#if 0//ndef USE_KERNEL_MODE
int TidToInstIdx(pthread_t tid);

void SetTidToInstIdx(pthread_t tid, int instIdx);
#endif
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SKIP_H_ */
