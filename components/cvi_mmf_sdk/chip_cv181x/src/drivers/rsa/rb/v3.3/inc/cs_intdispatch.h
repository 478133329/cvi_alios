/*
 * Copyright (C) 2017-2020 Alibaba Group Holding Limited
 */
/* cs_intdispatch.h
 *
 * Configuration Settings for the Interrupt Dispatcher for the CM FPGA.
 */



#include "cs_driver.h"
#include "cs_adapter.h"

// Definition of interrupt resources
// the interrupt sources is a bitmask of interrupt sources
// Refer to the data sheet of device for the correct values
// Config: Choose from RISING_EDGE, FALLING_EDGE, ACTIVE_HIGH, ACTIVE_LOW
//                            Name                  Inputs,   Config
#define INTDISPATCH_RESOURCES \
    INTDISPATCH_RESOURCE_ADD(ADAPTER_EIP28_READY_INT_NAME, \
                             1<<ADAPTER_PHY_EIP28_READY_IRQ, \
                             RISING_EDGE)


// select which interrupts to trace
// comment-out or set to zero to disable tracing
#define INTDISPATCHER_TRACE_FILTER  0x00000000

/* end of file cs_intdispatch.h */
