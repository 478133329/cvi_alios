/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2022. All rights reserved.
 *
 * File Name: cvi_ispd2_event_server.h
 * Description:
 */

#ifndef _CVI_ISPD2_EVENT_SERVER_H_
#define _CVI_ISPD2_EVENT_SERVER_H_

#include <stdio.h>
#include <stdint.h>

#include "cvi_ispd2_local.h"

// -----------------------------------------------------------------------------
CVI_S32 CVI_ISPD2_ES_CreateService(TISPDaemon2Info *ptObject, uint16_t u16ServicePort);
CVI_S32 CVI_ISPD2_ES_RunService(TISPDaemon2Info *ptObject);
CVI_S32 CVI_ISPD2_ES_DestoryService(TISPDaemon2Info *ptObject);

#endif // _CVI_ISPD2_EVENT_SERVER_H_
