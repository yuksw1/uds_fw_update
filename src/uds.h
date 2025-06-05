#ifndef __DSP_UDS_H__
#define __DSP_UDS_H__

#include <stdint.h>
#include "uds_hal.h"

void uds_init (void);
void uds_parse(uint8_t *data, uint16_t size);
void uds_poll (void);

#endif
