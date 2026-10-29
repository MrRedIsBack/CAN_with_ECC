#include <stdio.h>

#include "soc/twai_struct.h"   // The memory-mapped C struct (TWAI) for the CAN peripheral
#include "soc/dport_reg.h"     // System clock gating and hardware reset registers
#include "soc/gpio_sig_map.h"  // Internal crossbar signal IDs (TWAI_TX_IDX, TWAI_RX_IDX)
#include "rom/gpio.h"          // Silicon ROM helpers to route pins through the GPIO Matrix

#include <inttypes.h>

#define CAN_CLK_EN_BIT (1U << 19)
#define CAN_RST_BIT (1U << 19)

void app_main(void)
{
    *(volatile uint32_t *)DPORT_PERIP_CLK_EN_REG |= CAN_CLK_EN_BIT;  // Enable the clock for the CAN peripheral
    *(volatile uint32_t *)DPORT_PERIP_RST_EN_REG &= ~CAN_RST_BIT;      // Reset the CAN peripheral

    printf("mod_reg Value: %" PRIu32 "\n", TWAI.mode_reg.val);
    printf("The above value should be 33, if it is not, then you are fucked, good luck.");
}