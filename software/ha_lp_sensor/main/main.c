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
#define BOOT_CNT_MAX 4

int main(void)
{
    int boot_cnt = 0;
    board_init();
    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }
    bflb_mtd_init();
    easyflash_init();
    staWiFiInit();
    //读取复位信息
    boot_cnt = flash_get_device_boot_cnt();
    //识别是否够配网数量
    if (boot_cnt<BOOT_CNT_MAX) {
        LOG_F("复位次数=%d\r\n", boot_cnt);
        boot_cnt++;
        flash_save_device_boot_cnt(boot_cnt);
    }
    else {
        //复位数量符合，
        LOG_I("启动配网\r\n");
        flash_save_device_boot_cnt(0);
        blufi_config_start();
    }

    // device_state_machine_start();
    vTaskStartScheduler();
    while (1) {

        bflb_mtimer_delay_ms(1000);
    }
}
