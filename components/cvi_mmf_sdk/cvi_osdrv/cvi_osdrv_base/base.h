#ifndef __CV183X_BASE_H__
#define __CV183X_BASE_H__

#include <cvi_base.h>

unsigned int cvi_base_read_chip_id(void);

unsigned int cvi_base_read_chip_version(void);
unsigned int cvi_base_read_chip_pwr_on_reason(void);

int base_core_init(void);
void base_core_exit(void);


#endif /* __CV183X_BASE_H__ */
