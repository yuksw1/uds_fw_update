#ifndef _FW_UPDATE_H_
#define _FW_UPDATE_H_

#ifdef __cplusplus
    extern "C" {
#endif

#include <inttypes.h>

#define SESSION_DEFAULT         0x01
#define SESSION_PROGRAMMING     0x02
#define SESSION_EXTENDED        0x03

#define SRV_SESSION_CONTROL     0x10
#define SRV_ECU_RESET           0x11
#define SRV_READ_DID            0x22
#define SRV_SECURITY_ACCESS     0x27
#define SRV_COMM_CONTROL        0x28
#define SRV_WRITE_DID           0x2E
#define SRV_TESTER_PRESENT      0x3E
#define SRV_CONTROL_DTC         0x85
#define SRV_ROUTINE_CONTROL     0x31
#define SRV_REQUEST_DOWNLOAD    0x34
#define SRV_TRANSFER_DATA       0x36
#define SRV_REQ_TRANSFER_EXIT   0x37

#define COMM_RX_ON_TX_ON        0x00
#define COMM_RX_ON_TX_OFF       0x01
#define COMM_RX_OFF_TX_ON       0x02
#define COMM_RX_OFF_TX_OFF      0x03

#define REQUEST_SEED            0x01
#define SEND_KEY                0x02

#define REQUEST_SEED_CUSTOM     0x05
#define SEND_KEY_CUSTOM         0x06

#define ROUTINE_START           0x01

#define ROUTINE_ERASE_MEMORY            0xFF00
#define ROUTINE_CHECK_PROG_DEPENDENCY   0xFF01
#define ROUTINE_CHECK_MEMORY            0x0200


#define ERROR_SERVICE_NOT_SUPPORTED                         0x11
#define ERROR_SUB_FUNCTION_NOT_SUPPORT                      0x12
#define ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT    0x13
#define ERROR_REQUEST_SEQUENCE                              0x24
#define ERROR_SECURITY_ACCESS_DENIED                        0x33
#define ERROR_INCORRECT_KEY                                 0x35
#define ERROR_REQUIRED_TIME_DELAY_NOT_EXPIRED               0x37
#define ERROR_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING   0x78
#define ERROR_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION       0x7F

#define DTC_ON  1
#define DTC_OFF 2

#define HARD_RESET  1


//void session_control (uint8_t session);
void uds_poll_client (void);
void fw_update_start (char *file);
void fw_update_schedule (void);
int is_fw_update_done (void);

#ifdef __cplusplus
    }
#endif

#endif
