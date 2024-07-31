/**
 * @file main.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-07-26
 *
 * @copyright Copyright (c) 2024
 *
*/
#include "bflb_mtimer.h"
#include "board.h"
#include "device_state.h"

#define DBG_TAG "MAIN"


int main(void)
{

    board_init();
    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }
    bflb_mtd_init();
    easyflash_init();
    staWiFiInit();
    //读取复位信息

    device_state_machine_start();
    vTaskStartScheduler();
    while (1) {

        bflb_mtimer_delay_ms(1000);
    }
}
