#include <stdio.h>
#include "uds.h"
#include "util.h"
#include "fw_update.h"

int main (void)
{
    uds_init();
    fw_update_start ("test.dat");
    while (!is_fw_update_done ())
    {
        uds_poll ();
        uds_poll_client ();
        fw_update_schedule ();
        os_delay (1);
    }
    return 0;
}
