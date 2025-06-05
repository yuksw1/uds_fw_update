#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "uds.h"
#include "util.h"
#include "fw_update.h"

#define FW_START_ADDR   0x1D0000
#define SEND_BLK_SIZE   1024

typedef struct
{
    uint8_t *buf;
    uint32_t len;
    uint32_t crc;
    uint32_t send_len;
    uint32_t tm;
    uint8_t  blk_cnt;
    uint32_t state;
    int done;
} fw_info_t;

static fw_info_t s_fw;
static int s_res;
static uint32_t s_key;

static char *err_str (uint8_t code)
{
    switch (code)
    {
        case 0x00: return "positive response";
        case 0x11: return "service not supported";
        case 0x12: return "sub function not supported";
        case 0x13: return "incorrect message length or invalid length";
        case 0x14: return "response too long";
        case 0x22: return "conditions not correct";
        case 0x24: return "request sequence error";
        case 0x31: return "request out of range";
        case 0x33: return "security access denied";
        case 0x35: return "invalid key";
        case 0x36: return "exceeded number of attempts";
        case 0x37: return "required time delay not expired";
        case 0x70: return "upload download not accepted";
        case 0x71: return "transfer data suspended";
        case 0x72: return "general programming failure";
        case 0x73: return "response pendingwrong block sequence counter";
        case 0x78: return "response pending";
        case 0x7E: return "sub function not supported in active session";
        case 0x7F: return "service not supported in active session";
        case 0x92: return "voltage too high";
        case 0x93: return "voltage too low";
    }
    return "unknown error code";
}

static void INT_tp_send (uint8_t *buf, uint32_t len)
{
    s_res = -1;
    uds_tp_send_client(buf, (uint16_t)len);
    s_fw.tm = os_get_tick();
    s_fw.state++;
}

static void parse_read_did (uint8_t *data, uint16_t size)
{
    if (size != 7)
    {
        printf ("client: read did response length error, expected = 7, received = %u\n", size);
        return;
    }
    if ((data[1] != 0xF1) && (data[2] != 0x95))
    {
        printf ("client: error, %s, %d\n", __FILE__, __LINE__);
        return;
    }
    printf ("client: sw ver = %c%c%c%c\n", data[3], data[4], data[5], data[6]);
}

#define POLYNOMIAL 0x1FFF8823

static uint32_t uds_calc_secure_access_key (uint32_t seedx, uint32_t seedy)
{
    uint32_t Tmpx, Tmpy, key;

    Tmpx = (uint32_t)((((seedx ^ POLYNOMIAL) * 1859775393u)) + 2840853838u);
    Tmpy = (uint32_t)((((seedy ^ POLYNOMIAL) * 1518500249u)) + 2400959708u);
    key = Tmpx ^ Tmpy;
    return key;
}

static void uds_parse_client (uint8_t *data, uint16_t size)
{
    uint8_t sid;
    uint16_t P2, P2_;
    uint32_t seed_x, seed_y;

    if (size < 2)
    {
        printf ("client: invalid response len: %u\n", size);
        return;
    }

    s_res = data[0];
    if (data[0] == 0x7F)
    {
        printf ("client: SID=%02X, %s\n", data[1], err_str (data[2]));
        return;
    }

    sid = data[0] & ~0x40;
    switch (sid)
    {
        case SRV_SESSION_CONTROL:
            P2  = (data[2] << 8) | data[3];
            P2_ = ((data[4] << 8) | data[5]) * 10;
            printf ("client: ok, session=%02X, P2=%u ms, P2*=%u ms\n", data[1], P2, P2_);
            break;
        case SRV_READ_DID:
            parse_read_did (data, size);
            break;

        case SRV_SECURITY_ACCESS:
            if (data[1] == REQUEST_SEED_CUSTOM)
            {
                seed_x = get_u32(&data[2]);
                seed_y = get_u32(&data[6]);
                s_key = uds_calc_secure_access_key (seed_x, seed_y);
                printf ("client: seed = %08X %08X, key = %08X\n", seed_x, seed_y, s_key);
            }
            break;

        case SRV_ECU_RESET:
        case SRV_COMM_CONTROL:
        case SRV_WRITE_DID:
        case SRV_TESTER_PRESENT:
        case SRV_CONTROL_DTC:
        case SRV_ROUTINE_CONTROL:
        case SRV_REQUEST_DOWNLOAD:
        case SRV_TRANSFER_DATA:
        case SRV_REQ_TRANSFER_EXIT:
            printf ("client: okay, SID=0x%02X\n", data[0] - 0x40);
            break;
    }
}

static void session_control (uint8_t session)
{
    uint8_t cmd[2];

    cmd[0] = SRV_SESSION_CONTROL;
    cmd[1] = session;
    INT_tp_send (cmd, sizeof(cmd));
}

static void dtc_control (uint8_t on_off)
{
    uint8_t cmd[2];

    cmd[0] = SRV_CONTROL_DTC;
    cmd[1] = on_off;
    INT_tp_send (cmd, sizeof(cmd));
}

static void communication_control (uint8_t on_off)
{
    uint8_t cmd[3];

    cmd[0] = SRV_COMM_CONTROL;
    cmd[1] = on_off;
    cmd[2] = 1;
    INT_tp_send (cmd, sizeof(cmd));
}

static void read_sw_ver (void)
{
    uint8_t cmd[3];

    cmd[0] = SRV_READ_DID;
    cmd[1] = 0xF1;
    cmd[2] = 0x95;
    INT_tp_send (cmd, sizeof(cmd));
}

static void request_seed (void)
{
    uint8_t cmd[2];

    cmd[0] = SRV_SECURITY_ACCESS;
    cmd[1] = REQUEST_SEED_CUSTOM;
    INT_tp_send (cmd, sizeof(cmd));
}

static void send_key (uint32_t key)
{
    uint8_t cmd[6];

    cmd[0] = SRV_SECURITY_ACCESS;
    cmd[1] = SEND_KEY_CUSTOM;
    cmd[2] = (uint8_t)((key >> (8 * 3)) & 0xFF);
    cmd[3] = (uint8_t)((key >> (8 * 2)) & 0xFF);
    cmd[4] = (uint8_t)((key >> (8 * 1)) & 0xFF);
    cmd[5] = (uint8_t)((key >> (8 * 0)) & 0xFF);
    INT_tp_send (cmd, sizeof(cmd));
}

static void erase_memory (uint32_t file_start_addr, uint32_t file_size)
{
    uint8_t cmd[13];

    cmd[0] = SRV_ROUTINE_CONTROL;
    cmd[1] = ROUTINE_START;
    cmd[2] = (uint8_t)(ROUTINE_ERASE_MEMORY >> 8);
    cmd[3] = (uint8_t)(ROUTINE_ERASE_MEMORY & 0xFF);
    cmd[4] = 0x44;
    put_u32(&cmd[5], file_start_addr);
    put_u32(&cmd[9], file_size);
    INT_tp_send (cmd, sizeof(cmd));
}

static void request_download (uint32_t file_start_addr, uint32_t file_size)
{
    uint8_t cmd[11];

    cmd[0] = SRV_REQUEST_DOWNLOAD;
    cmd[1] = 0x00;
    cmd[2] = 0x44;
    put_u32(&cmd[3], file_start_addr);
    put_u32(&cmd[7], file_size);
    INT_tp_send (cmd, sizeof(cmd));
    s_fw.send_len = 0;
}

static void transfer_data (void *buf, uint32_t len)
{
    static uint8_t cmd[2 + 4095];

    if (len > 4095)
    {
        printf ("transfer_data: length error (%u)\n", len);
        return;
    }
    cmd[0] = SRV_TRANSFER_DATA;
    cmd[1] = s_fw.blk_cnt;
    s_fw.blk_cnt++;
    memcpy (&cmd[2], buf, len);
    INT_tp_send (cmd, len + 2);
}

static void request_transfer_exit (void)
{
    uint8_t cmd[1];

    cmd[0] = SRV_REQ_TRANSFER_EXIT;
    INT_tp_send (cmd, sizeof(cmd));
}

static void check_memory (uint32_t start_addr, uint32_t size, uint32_t crc)
{
    uint8_t cmd[18];

    cmd[0] = SRV_ROUTINE_CONTROL;
    cmd[1] = ROUTINE_START;
    cmd[2] = (uint8_t)(ROUTINE_CHECK_MEMORY >> 8);
    cmd[3] = (uint8_t)(ROUTINE_CHECK_MEMORY & 0xFF);
    put_u32(&cmd[4], start_addr);
    put_u32(&cmd[8], size);
    put_u16(&cmd[12], sizeof(crc));
    put_u32(&cmd[14], crc);
    INT_tp_send (cmd, sizeof(cmd));
}

static void check_prog_dependency (void)
{
    uint8_t cmd[4];

    cmd[0] = SRV_ROUTINE_CONTROL;
    cmd[1] = ROUTINE_START;
    cmd[2] = (uint8_t)(ROUTINE_CHECK_PROG_DEPENDENCY >> 8);
    cmd[3] = (uint8_t)(ROUTINE_CHECK_PROG_DEPENDENCY & 0xFF);
    INT_tp_send (cmd, sizeof(cmd));
}

static void ecu_reset (uint8_t reset_type)
{
    uint8_t cmd[2];

    cmd[0] = SRV_ROUTINE_CONTROL;
    cmd[1] = reset_type;
    INT_tp_send (cmd, sizeof(cmd));
}

/*********************************************************************/
/*********************************************************************/
/*********************************************************************/

void uds_poll_client (void)
{
    static uint8_t buf[4096];
    uint16_t len;

    len = uds_tp_receive_client(buf);
    if (len > 0)
    {
        uds_parse_client(buf, len);
    }
}

static uint32_t file_length (FILE *fp)
{
    struct stat buf;

    fstat(fileno(fp), &buf);
    return (uint32_t)buf.st_size;
}

void fw_update_start (char *file)
{
    FILE *fp;
    uint32_t len;

    fp = fopen (file, "rb");
    if (fp != NULL)
    {
        len = file_length (fp);
        if (len > 0)
        {
            s_fw.buf = malloc (len);
            if (s_fw.buf != NULL)
            {
                len = fread (s_fw.buf, 1, len, fp);
                fclose (fp);
                s_fw.len = len;
                s_fw.send_len = 0;
                s_fw.state = 10;
                s_fw.crc = make_crc32(0xFFFFFFFF, s_fw.buf, len);
                s_fw.done = 0;
            }
        }
    }
    else
    {
        printf ("[%s] open fail\n", file);
    }
}

static void wait_response (void)
{
    if ((os_get_tick() - s_fw.tm) >= 1000)
    {
        printf ("response timeout\n");
        s_fw.done = 1;
        s_fw.state = 0;
    }
    if (s_res != -1)
    {
        if (s_res == 0x7F)
        {
            printf ("error, abort\n");
            s_fw.done = 1;
            s_fw.state = 0;
        }
        else
        {
            s_fw.state++;
        }
    }
}

void fw_update_schedule (void)
{
    uint32_t blk_len;

    {
        static uint32_t prev_step;

        if (prev_step != s_fw.state)
        {
            prev_step = s_fw.state;
            printf ("fw_update: step = %u\n", prev_step);
        }
    }
    switch (s_fw.state)
    {
        case 0:
            break;

        case 10:
            session_control (SESSION_DEFAULT);
            break;
        case 11:
            wait_response ();
            break;
        case 12:
            read_sw_ver ();
            break;
        case 13:
            wait_response ();
            break;
        case 14:
            session_control (SESSION_EXTENDED);
            break;
        case 15:
            wait_response ();
            break;
        case 16:
            dtc_control (DTC_OFF);
            break;
        case 17:
            wait_response ();
            break;
        case 18:
            communication_control (COMM_RX_OFF_TX_OFF);
            break;
        case 19:
            wait_response ();
            break;
        case 20:
            session_control (SESSION_PROGRAMMING);
            break;
        case 21:
            wait_response ();
            break;
        case 22:
            printf ("wait 1.5 sec\n");
            s_fw.tm = os_get_tick();
            s_fw.state++;
            break;
        case 23:
            if ((os_get_tick() - s_fw.tm) >= 1500)
            {
                s_fw.state++;
            }
            break;
        case 24:
            request_seed ();
            break;
        case 25:
            wait_response ();
            break;
        case 26:
            send_key (s_key);
            break;
        case 27:
            wait_response ();
            break;
        case 28:
            erase_memory (FW_START_ADDR, s_fw.len);
            break;
        case 29:
            wait_response ();
            break;
        case 30:
            request_download (FW_START_ADDR, s_fw.len);
            break;
        case 31:
            wait_response ();
            break;
        case 32:
            blk_len = my_min ((s_fw.len - s_fw.send_len), SEND_BLK_SIZE);
            transfer_data (&s_fw.buf[s_fw.send_len], blk_len);
            s_fw.send_len += blk_len;
            break;
        case 33:
            wait_response ();
            break;
        case 34:
            if (s_fw.len == s_fw.send_len)
            {
                s_fw.state++;
            }
            else
            {
                s_fw.state -= 2;
            }
            break;
        case 35:
            request_transfer_exit ();
            break;
        case 36:
            wait_response ();
            break;
        case 37:
            check_memory (FW_START_ADDR, s_fw.len, s_fw.crc);
            break;
        case 38:
            wait_response ();
            break;
        case 39:
            check_prog_dependency ();
            break;
        case 40:
            wait_response ();
            break;
        case 41:
            session_control (SESSION_EXTENDED);
            break;
        case 42:
            wait_response ();
            break;
        case 43:
            ecu_reset (HARD_RESET);
            break;
        case 44:
            wait_response ();
            break;
        case 45:
            printf ("done\n");
            s_fw.done = 1;
            s_fw.state = 0;
            break;
    }
}

int is_fw_update_done (void)
{
    return s_fw.done;
}
