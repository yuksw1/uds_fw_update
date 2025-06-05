#include <stdio.h>
#include "uds.h"

#define SESSION_TIMEOUT             5000
#define SECURITY_ACCESS_DELAY_TIME  10000

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

typedef struct {
    int      secure;
    uint8_t  session;
    uint8_t  service;
    uint8_t  sub_func;
    uint8_t  blk_cnt;
    uint32_t security_seed_x;
    uint32_t security_seed_y;
    uint32_t security_key;
    uint32_t tm_session;
    uint32_t tm_security_delay;
} uds_info_t;

static uds_info_t s_uds;
static FILE *outFile = NULL; // Added for firmware data saving

void c_printf (const char *format, ...);

static uint16_t get_u16(void *p)
{
    uint8_t *b = p;

    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

static uint32_t get_u32(void *p)
{
    uint8_t *b = p;

    return (uint32_t)(((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3]);
}

static uint32_t gen_random(void)
{
    uint32_t num = uds_get_ms();

    num ^= num << 13;
    num ^= num >> 17;
    num ^= num << 5;
    if (num > 0xB5000000)
    {
        num += 0x01F639CD;
    }
    else
    {
        num += 0x4AC26813;
    }

    if (num == 0)
    {
        num = 0x39CDAC26;
    }
    return num;
}

#define POLYNOMIAL  0x1FFF8823

static uint32_t gen_security_key(uint32_t seedx, uint32_t seedy)
{
    uint32_t Tmpx = (uint32_t)((((seedx ^ POLYNOMIAL) * 1859775393u)) + 2840853838u);
    uint32_t Tmpy = (uint32_t)((((seedy ^ POLYNOMIAL) * 1518500249u)) + 2400959708u);
    uint32_t key = Tmpx ^ Tmpy;
    return key;
}

static int send_positive_response(uint8_t *payload, uint16_t size)
{
    static uint8_t msg[128];

    msg[0] = s_uds.service + 0x40;
    msg[1] = s_uds.sub_func;

//    c_printf ("pos res: %02X %02X", msg[0], msg[1]);
    for(uint16_t Index = 0; Index < size; ++Index)
    {
        msg[2 + Index] = payload[Index];
//        c_printf (" %02X", payload[Index]);
    }
//    c_printf ("\n");
    return uds_tp_send(msg, (size + 2));
}

static int send_negative_response (uint8_t nrc)
{
    static uint8_t msg[128];

    msg[0] = 0x7F;
    msg[1] = s_uds.service;
    msg[2] = nrc;
    return uds_tp_send(msg, 3);
}

static void session_timeout_check (void)
{
    if(s_uds.session != SESSION_DEFAULT)
    {
        if(s_uds.tm_session < uds_get_ms())
        {
            c_printf ("session timeout %ums\n", uds_get_ms() - s_uds.tm_session); // Uncommented log
            s_uds.session = SESSION_DEFAULT;
            s_uds.security_seed_x = gen_random();
            s_uds.security_seed_y = gen_random();
            s_uds.security_key = 0;
        }
        else s_uds.tm_session = uds_get_ms() + SESSION_TIMEOUT;
    }
}

static void srv_session_control (uint8_t *data, uint16_t size)
{
    uint8_t msg[] = { 0x00, 0x32, 0x01, 0xF4 };

    s_uds.sub_func = data[1];
    if (size != 2)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    else
    {
        switch(s_uds.sub_func)
        {
            case SESSION_DEFAULT:
                c_printf ("SESSION_DEFAULT\n");
                s_uds.session = SESSION_DEFAULT;
                s_uds.tm_session = uds_get_ms() + SESSION_TIMEOUT;
                send_positive_response(msg, 4);
                break;

            case SESSION_PROGRAMMING:
                c_printf ("SESSION_PROGRAMMING\n");
                s_uds.session = SESSION_PROGRAMMING;
                s_uds.tm_session = uds_get_ms() + SESSION_TIMEOUT;
                send_positive_response(msg, 4);
                break;

            case SESSION_EXTENDED:
                c_printf ("SESSION_EXTENDED\n");
                s_uds.session = SESSION_EXTENDED;
                s_uds.tm_session = uds_get_ms() + SESSION_TIMEOUT;
                send_positive_response(msg, 4);
                break;

            default:
                send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
                break;
        }
    }
    return;
}

static void srv_ecu_reset (uint8_t *data, uint16_t size)
{
    s_uds.sub_func = data[1];
    if (size != 2)
    {
        send_negative_response(ERROR_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    }
    else
    {
        if (data[1] == HARD_RESET)
        {
            c_printf ("Hard reset\n");
            send_positive_response(NULL, 0);
        }
        else
        {
            send_negative_response(ERROR_SERVICE_NOT_SUPPORTED);
        }
    }
}

static void srv_read_did (uint8_t *data, uint16_t size)
{
    uint8_t msg[7];
    uint16_t data_id;

    s_uds.sub_func = data[1];
    if (size != 3)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    data_id = get_u16(&data[1]);
    c_printf ("Read DID 0x%04X\n", data_id);
    if (data_id == 0xF195)
    {
        msg[0] = data[0] + 0x40;
        msg[1] = data[1];
        msg[2] = data[2];

        /* software version */
        msg[3] = '1';
        msg[4] = '2';
        msg[5] = '3';
        msg[6] = '4';
        uds_tp_send(msg, sizeof(msg));
    }
    else
    {
        send_negative_response(ERROR_SERVICE_NOT_SUPPORTED);
    }
}

static void srv_security_access (uint8_t *data, uint16_t size)
{
    uint8_t msg[8];
    uint32_t requested_key;

    s_uds.sub_func = data[1];
    if(s_uds.tm_security_delay > uds_get_ms())
    {
        send_negative_response(ERROR_REQUIRED_TIME_DELAY_NOT_EXPIRED);
        return;
    }

    if(s_uds.session == SESSION_DEFAULT)
    {
        send_negative_response(ERROR_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
        return;
    }

    switch(s_uds.sub_func)
    {
        case REQUEST_SEED_CUSTOM:
            if (size != 2)
            {
                send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
                return;
            }
            msg[0] = (uint8_t)(s_uds.security_seed_x >> 24);
            msg[1] = (uint8_t)(s_uds.security_seed_x >> 16);
            msg[2] = (uint8_t)(s_uds.security_seed_x >> 8);
            msg[3] = (uint8_t)(s_uds.security_seed_x);
            msg[4] = (uint8_t)(s_uds.security_seed_y >> 24);
            msg[5] = (uint8_t)(s_uds.security_seed_y >> 16);
            msg[6] = (uint8_t)(s_uds.security_seed_y >> 8);
            msg[7] = (uint8_t)(s_uds.security_seed_y);
            s_uds.security_key = gen_security_key(s_uds.security_seed_x, s_uds.security_seed_y);
            c_printf ("SECURITY_ACCESS, seed = 0x%08X %08X, key = %08X", s_uds.security_seed_x, s_uds.security_seed_y, s_uds.security_key);
            send_positive_response(msg, 8);
            break;

        case SEND_KEY_CUSTOM:
            if(size != 6)
            {
                send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
                break;
            }
            if ((s_uds.security_seed_x != 0) && (s_uds.security_seed_y != 0))
            {
                requested_key = data[2];
                requested_key = (requested_key << 8) | data[3];
                requested_key = (requested_key << 8) | data[4];
                requested_key = (requested_key << 8) | data[5];
                c_printf ("SERVER requested key: %08X, %02X %02X %02X %02X\n", requested_key, data[2], data[3], data[4], data[5]);

                s_uds.security_key = gen_security_key(s_uds.security_seed_x, s_uds.security_seed_y);
                c_printf ("SECURITY_ACCESS, key = 0x%08X, 0x%08X\n", requested_key, s_uds.security_key);
                if(requested_key != s_uds.security_key)
                {
                    s_uds.tm_security_delay = uds_get_ms() + SECURITY_ACCESS_DELAY_TIME;
                    s_uds.security_seed_x = gen_random();
                    s_uds.security_seed_y = gen_random();
                    s_uds.security_key = 0;
                    send_negative_response(ERROR_INCORRECT_KEY);
                    break;
                }
                s_uds.secure = 1;
                s_uds.security_seed_x = 0;
                s_uds.security_seed_y = 0;
                send_positive_response(NULL, 0);
            }
            break;

        default:
            send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
            break;
    }
}

static void srv_control_dtc_setting (uint8_t *data, uint16_t size)
{
    s_uds.sub_func = data[1];
    if (size != 2)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    s_uds.sub_func = data[1];
    if (s_uds.sub_func == DTC_ON)
    {
        c_printf ("DTC on\n");
        send_positive_response(NULL, 0);
    }
    else if (s_uds.sub_func == DTC_OFF)
    {
        c_printf ("DTC off\n");
        send_positive_response(NULL, 0);
    }
    else
    {
        send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
    }
}

static void srv_communication_control (uint8_t *data, uint16_t size)
{
    s_uds.sub_func = data[1];
    if (size != 3)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (data[2] != 0x01)
    {
        send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
        return;
    }

    s_uds.sub_func = data[1];
    switch (s_uds.sub_func)
    {
        case COMM_RX_ON_TX_ON:
            c_printf ("Communication Control: Rx(on), Tx(on)\n");
            break;
        case COMM_RX_ON_TX_OFF:
            c_printf ("Communication Control: Rx(on), Tx(off)\n");
            break;
        case COMM_RX_OFF_TX_ON:
            c_printf ("Communication Control: Rx(off), Tx(on)\n");
            break;
        case COMM_RX_OFF_TX_OFF:
            c_printf ("Communication Control: Rx(off), Tx(off)\n");
            break;
        default:
            send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
            return;
    }
    send_positive_response(NULL, 0);
}

static void srv_routine_control_erase_memory (uint8_t *data, uint16_t size)
{
    uint8_t msg[3];
    uint32_t file_start_addr, file_size;

    s_uds.sub_func = data[1];
    if ((size != 13) || (data[4] != 0x44))
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }
    file_start_addr = get_u32(&data[5]);
    file_size       = get_u32(&data[9]);
    c_printf ("file_start_addr = 0x%08X, file_size = 0x%08X\n", file_start_addr, file_size);
    msg[0] = data[2];
    msg[1] = data[3];
    msg[2] = 0; // 0=success, 1=error
    send_positive_response(msg, 3);
    //send_negative_response(ERROR_REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING);
}

static void srv_routine_control_check_memory (uint8_t *data, uint16_t size)
{
    uint8_t msg[8];
    uint32_t mem_addr, mem_size, crc;
    uint16_t crc_len;

    s_uds.sub_func = data[1];
    if (size != 18)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    mem_addr = get_u32(&data[4]);
    mem_size = get_u32(&data[8]);
    crc_len = get_u16(&data[12]);
    if (crc_len != 4)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }
    crc = get_u32(&data[14]);
    c_printf ("check memory: addr = 0x%08X, len = %08X, crc = 0x%08X\n", mem_addr, mem_size, crc);

    msg[0] = 0x71;
    msg[1] = 0x01;
    msg[2] = 0x02;
    msg[3] = 0x00;

    msg[4] = 0x00; // 0=success, 1 = error
    msg[5] = 0x00; // 0=success, 1 = error
    msg[6] = 0x00; // 0=success, 1 = error
    msg[7] = 0x00; // 0=success, 1 = error
    uds_tp_send(msg, 8);
}

static void srv_routine_control_check_programming_dependency (uint8_t *data, uint16_t size)
{
    uint8_t msg[8];

    s_uds.sub_func = data[1];
    if (size != 4)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }
    msg[0] = 0x71;
    msg[1] = 0x01;
    msg[2] = 0xFF;
    msg[3] = 0x01;
    msg[4] = 0x00; // 0=success, 1 = error
    msg[5] = 0x00; // 0=success, 1 = error
    msg[6] = 0x00; // 0=success, 1 = error
    msg[7] = 0x00; // 0=success, 1 = error
    uds_tp_send(msg, 8);
}

static void srv_routine_control (uint8_t *data, uint16_t size)
{
    uint16_t routine_id;

    s_uds.sub_func = data[1];
    switch (s_uds.sub_func)
    {
        case ROUTINE_START:
            routine_id = get_u16(&data[2]);
            switch (routine_id)
            {
                case ROUTINE_ERASE_MEMORY:
                    srv_routine_control_erase_memory (data, size);
                    break;
                case ROUTINE_CHECK_MEMORY:
                    srv_routine_control_check_memory (data, size);
                    break;
                case ROUTINE_CHECK_PROG_DEPENDENCY:
                    srv_routine_control_check_programming_dependency (data, size);
                    break;
                default:
                    send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
                    break;
            }
            break;
        default:
            send_negative_response(ERROR_SUB_FUNCTION_NOT_SUPPORT);
            break;
    }
}

static void srv_request_download (uint8_t *data, uint16_t size)
{
    uint8_t msg[4];
    uint32_t file_start_addr, file_size;
    uint8_t max_num_of_block_len = 2;

    s_uds.sub_func = data[1];
    if ((size != 11) || (data[1] != 0) || (data[2] != 0x44))
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }
    file_start_addr = get_u32(&data[3]);
    file_size       = get_u32(&data[7]);
    s_uds.blk_cnt = 1;
    c_printf ("request download, file_start_addr = 0x%08X, file_size = 0x%08X\n", file_start_addr, file_size);

    /***********************************************/
    msg[0] = s_uds.service + 0x40;
    msg[1] = (uint8_t)(max_num_of_block_len << 4); // length format identifier

    /* maxNumberOfBlockLength 0xF02(3842) */
    msg[2] = 0x0F;
    msg[3] = 0x02;
    uds_tp_send(msg, 4);
}

static void srv_transfer_data (uint8_t *data, uint16_t size)
{
    uint8_t seq = data[1];
    uint8_t *p = &data[2];

    if (size < 3)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (seq == 1)
    {
        if (outFile != NULL)
        {
            fclose(outFile); // Close if already open (e.g., interrupted transfer)
            outFile = NULL;
        }
        outFile = fopen("out.dat", "wb");
        if (outFile == NULL)
        {
            // perror is not available, use c_printf
            c_printf("SERVER: Error opening out.dat for writing.\n");
            send_negative_response(0x72); // General Programming Failure
            return;
        }
    }
    else if (outFile == NULL)
    {
        // This means we missed the first block or an error occurred after opening
        c_printf("SERVER: outFile is NULL for seq > 1.\n");
        send_negative_response(ERROR_REQUEST_SEQUENCE);
        return;
    }

    if (s_uds.blk_cnt != seq)
    {
        c_printf("SERVER: Sequence error. Expected: %u, Got: %u.\n", s_uds.blk_cnt, seq);
        send_negative_response(ERROR_REQUEST_SEQUENCE);
        // Do not close outFile here, as a new transfer might start
        return;
    }

    if (fwrite(p, 1, size - 2, outFile) != (size_t)(size - 2))
    {
        c_printf("SERVER: Error writing to outFile.\n");
        fclose(outFile);
        outFile = NULL;
        send_negative_response(0x72); // General Programming Failure
        return;
    }
    fflush(outFile); // Ensure data is written to disk

    s_uds.blk_cnt++;
    c_printf ("transfer data: seq = %u, data[] = %02X %02X ..., len = %u\n", seq, p[0], p[1], size - 2);
    send_positive_response(NULL, 0);
}

static void srv_req_transfer_exit (uint8_t *data, uint16_t size)
{
    // s_uds.sub_func = data[1]; // No sub-function for RequestTransferExit, and data[1] is out of bounds if size is 1.
                                // For this service, sub_func is often ignored or considered 0.
    if (size != 1)
    {
        send_negative_response(ERROR_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    if (outFile != NULL)
    {
        fclose(outFile);
        outFile = NULL;
    }

    send_positive_response(NULL, 0); // Send positive response (SID + 0x40)
}

/*
    defa | ext | prg |
   ------+-----+-----+--------------------------
      o  |  o  |  o  |  0x3E Tester Present
      o  |  o  |  o  |  0x10 Session Control
      o  |  o  |  o  |  0x11 ECU Reset
      o  |     |     |  0x22 Read DID
         |  o  |     |  0x85 DTC Setting
         |  o  |     |  0x28 Communication Control
         |  o  |  o  |  0x27 Security Access (Seed / Key)
         |  o  |  o  |  0x31 Routine control (Erase Memory / Check Memory / Check Programming Dependency)
         |     |  o  |  0x34 Request Download
         |     |  o  |  0x36 Transfer Data
         |     |  o  |  0x37 Request Transfer Exit

step
  1  SECC Power On
  2  Wait 5 sec
  3  StartDiagnosticSession (Default session)
  4  ReadDataIdentifier (Read Software Version)
  5  StartDiagnosticSession (Extended session)
  6  ControlDTCSetting, OFF
  7  CommunicationControl, DisableRx/Tx
  8  StartDiagnosticSession (Programming session)
  9  Wait 1.5 Sec
 10  SecureAccess (request seed)
 11  SecureAccess (send key)
 12  Erase Memory (file start addr(0x1D000000), size(x))
 13  RequestDownload (file start addr(0x1D000000), size(x))
 14  TransferData (data)
 15  RequestTransferExit
 16  CheckMemory (mem addr(0x1D000000), mem size(x), verify data len=4, verify data(x))
 17  CheckProgrammingDependency(31 01 FF 01)
 18  Extended session
 19  ECU Reset
*/

static int check_session (uint8_t *data, uint16_t size)
{
    switch (s_uds.service)
    {
        case SRV_TESTER_PRESENT:
        case SRV_SESSION_CONTROL:
        case SRV_ECU_RESET:
        case SRV_READ_DID:
            break;

        case SRV_SECURITY_ACCESS:
        case SRV_CONTROL_DTC:
        case SRV_COMM_CONTROL:
        case SRV_ROUTINE_CONTROL:
        case SRV_REQUEST_DOWNLOAD:
        case SRV_TRANSFER_DATA:
        case SRV_REQ_TRANSFER_EXIT:
            if (s_uds.session == SESSION_DEFAULT)
            {
c_printf ("%s, %d\n", __FILE__, __LINE__);
                send_negative_response(ERROR_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
                return 1;
            }
            break;

        default:
c_printf ("%s, %d\n", __FILE__, __LINE__);
            send_negative_response(ERROR_SERVICE_NOT_SUPPORTED);
            return 1;
    }
    return 0;
}

static int check_security (uint8_t *data, uint16_t size)
{
    switch (s_uds.service)
    {
        case SRV_ROUTINE_CONTROL:
        case SRV_REQUEST_DOWNLOAD:
        case SRV_TRANSFER_DATA:
        case SRV_REQ_TRANSFER_EXIT:
            if (s_uds.secure == 0)
            {
c_printf ("%s, %d\n", __FILE__, __LINE__);
                send_negative_response(ERROR_SECURITY_ACCESS_DENIED);
                return 1;
            }
            break;
        default:
            break;
    }
    return 0;
}

static int check_message (uint8_t *data, uint16_t size)
{
    if (check_session (data, size) == 0)
    {
        if (check_security (data, size) == 0)
        {
            return 0;
        }
    }
    return 1;

}

void uds_parse(uint8_t *data, uint16_t size)
{
    session_timeout_check ();
    s_uds.service = data[0];
    if (check_message (data, size) == 0)
    {
        switch (s_uds.service)
        {
            case SRV_SESSION_CONTROL:
                srv_session_control (data, size);
                break;

            case SRV_ECU_RESET:
                srv_ecu_reset (data, size);
                break;

            case SRV_READ_DID:
                srv_read_did (data, size);
                break;

            case SRV_SECURITY_ACCESS:
                srv_security_access (data, size);
                break;

            case SRV_TESTER_PRESENT:
              s_uds.sub_func = data[1];
              send_positive_response(NULL, 0);
              break;

            case SRV_CONTROL_DTC:
                srv_control_dtc_setting (data, size);
                break;

            case SRV_COMM_CONTROL:
                srv_communication_control (data, size);
                break;

            case SRV_ROUTINE_CONTROL:
                srv_routine_control (data, size);
                break;

            case SRV_REQUEST_DOWNLOAD:
                srv_request_download (data, size);
                break;

            case SRV_TRANSFER_DATA:
                srv_transfer_data (data, size);
                break;

            case SRV_REQ_TRANSFER_EXIT:
                srv_req_transfer_exit (data, size);
                break;

            default:
                send_negative_response(ERROR_SERVICE_NOT_SUPPORTED);
                break;
        }
    }
}

void uds_poll (void)
{
    static uint8_t buf[4096];
    uint16_t len;

    len = uds_tp_receive(buf);
    if (len > 0)
    {
        uds_parse(buf, len);
    }
}

void uds_init (void)
{
    uds_hal_init();
}
