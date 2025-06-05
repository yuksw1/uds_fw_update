#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include "uds_hal.h"

static uint8_t tp_buf_from_server[4096];
static uint8_t tp_buf_from_client[4096];
static uint16_t tp_len_from_server;
static uint16_t tp_len_from_client;

void c_printf (const char *format, ...)
{
    va_list argp;

    va_start (argp, format);
    vprintf (format, argp);
    va_end (argp);
}

int uds_tp_send(uint8_t *payload, uint16_t size)
{
    if (tp_len_from_server != 0)
    {
        printf ("uds_tp_send_server(), full\n");
        return 0;
    }
    memcpy (tp_buf_from_server, payload, size);
    tp_len_from_server = size;

{
    uint16_t i;

    c_printf ("TP Tx:");
    for (i = 0; i < size; i++)
    {
        c_printf (" %02X", payload[i]);
    }
    c_printf ("\n");
}

    return 0;
}

int uds_tp_receive(uint8_t *payload)
{
    uint16_t len = tp_len_from_client;

    if (len > 0)
    {
        memcpy (payload, tp_buf_from_client, len);
        tp_len_from_client = 0;;
    }
    return len;
}

int uds_tp_send_client(uint8_t *payload, uint16_t size)
{
    if (tp_len_from_client != 0)
    {
        printf ("uds_tp_send_client(), full\n");
        return 0;
    }
    memcpy (tp_buf_from_client, payload, size);
    tp_len_from_client = size;
    return 0;
}

int uds_tp_receive_client(uint8_t *payload)
{
    uint16_t len = tp_len_from_server;

    if (len > 0)
    {
        memcpy (payload, tp_buf_from_server, len);
        tp_len_from_server = 0;;
    }
    return len;
}

uint32_t uds_get_ms(void)
{
    return os_get_tick();
}

void uds_hal_init (void)
{
}
