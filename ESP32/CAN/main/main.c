#include <stdio.h>

#include "soc/twai_struct.h"   // The memory-mapped C struct (TWAI) for the CAN peripheral
#include "soc/dport_reg.h"     // System clock gating and hardware reset registers
#include "soc/gpio_struct.h"    // The memory-mapped C struct (GPIO) for the GPIO peripheral
#include "soc/io_mux_reg.h"  // The memory-mapped C struct (IO_MUX) for the GPIO peripheral
#include "soc/gpio_sig_map.h" 

#include <inttypes.h>

#define CAN_CLK_EN_BIT (1U << 19)
#define CAN_RST_BIT (1U << 19)

#define GPIO_RX_BIT 22
#define GPIO_TX_BIT 21

void app_main(void)
{
    *(volatile uint32_t *)DPORT_PERIP_CLK_EN_REG |= CAN_CLK_EN_BIT;  // Enable the clock for the CAN peripheral
    *(volatile uint32_t *)DPORT_PERIP_RST_EN_REG &= ~CAN_RST_BIT;      // Reset the CAN peripheral

    printf("mod_reg Value: %" PRIu32 "\n", TWAI.mode_reg.val);
    printf("The above value should be 33, if it is not, then you are fucked, good luck. \n");

    // No clue what these 3 lines are doing
    PIN_FUNC_SELECT(IO_MUX_GPIO21_REG, FUNC_GPIO21_GPIO21);
    PIN_FUNC_SELECT(IO_MUX_GPIO22_REG, FUNC_GPIO22_GPIO22);
    PIN_INPUT_ENABLE(IO_MUX_GPIO22_REG);
    
    // Output Enable the GPIO Pins
    GPIO.enable_w1tc = (1U << GPIO_RX_BIT); // turns on GPIO22 - RX - input enable
    GPIO.enable_w1ts = (1U << GPIO_TX_BIT); // turns on GPIO21 - TX - output enable

    GPIO.func_out_sel_cfg[GPIO_TX_BIT].func_sel = 123; // Set GPIO21 to output the CAN TX signal (index 123)
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].func_sel = GPIO_RX_BIT; // Set the CAN RX signal (index 94) to be input for GPIO22
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].sig_in_sel = 1; // Bypass the IO matrix for GPIO22 input
}