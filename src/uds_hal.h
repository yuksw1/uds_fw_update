#ifndef _UDS_HAL_H_
#define _UDS_HAL_H_

#ifdef __cplusplus
    extern "C" {
#endif

#include <inttypes.h>

int uds_tp_send(uint8_t *payload, uint16_t size);
int uds_tp_receive(uint8_t *payload);
uint32_t uds_get_ms(void);
void uds_hal_init (void);
int uds_tp_send_client(uint8_t *payload, uint16_t size);
int uds_tp_receive_client(uint8_t *payload);

#ifdef __cplusplus
    }
#endif

#endif
