#include <stdio.h>

#include "soc/twai_struct.h"   // The memory-mapped C struct (TWAI) for the CAN peripheral
#include "soc/dport_reg.h"     // System clock gating and hardware reset registers
#include "soc/gpio_struct.h"    // The memory-mapped C struct (GPIO) for the GPIO peripheral
#include "soc/io_mux_reg.h"  // The memory-mapped C struct (IO_MUX) for the GPIO peripheral
#include "soc/gpio_sig_map.h" 

#include <inttypes.h>

#define CAN_CLK_EN_BIT (1U << 19)
#define CAN_RST_BIT (1U << 19)

#define GPIO_TX_PIN 21
#define GPIO_RX_PIN 22
#define GPIO_TX_BIT (1U << GPIO_TX_PIN)
#define GPIO_RX_BIT (1U << GPIO_RX_PIN)

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
    GPIO.enable_w1tc = GPIO_RX_PIN; // turns on GPIO22 - RX - input enable
    GPIO.enable_w1ts = GPIO_TX_PIN; // turns on GPIO21 - TX - output enable

    GPIO.func_out_sel_cfg[GPIO_TX_BIT].func_sel = 123; // Set GPIO21 to output the CAN TX signal (index 123)
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].func_sel = GPIO_RX_PIN; // Set the CAN RX signal (index 94) to be input for GPIO22
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].sig_in_sel = 1; // Bypass the IO matrix for GPIO22 input

    // Set the Reset Mode bit in the mode
    TWAI.mode_reg.rm = 1;

    // Set the register layout to extended
    TWAI.clock_divider_reg.cm = 1;
    TWAI.clock_divider_reg.co = 1; // Disable the clock output

    // Configure BAUD rate to 500 kbps and Synchronization Jump Width to 3Tq wide
    TWAI.bus_timing_0_reg.brp = 0x03;
    TWAI.bus_timing_0_reg.sjw = 0x2;

    TWAI.bus_timing_1_reg.tseg1 = 0xE;
    TWAI.bus_timing_1_reg.tseg2 = 0x3;
    TWAI.bus_timing_1_reg.sam = 1; // Triple sampling

    // Turn on single filter mode
    TWAI.mode_reg.afm = 1;

    // Set the acceptance bit mask to 1 so that everything is accepted
    TWAI.acceptance_filter.amr[0].byte = 0xFF; 
    TWAI.acceptance_filter.amr[1].byte = 0xFF; 
    TWAI.acceptance_filter.amr[2].byte = 0xFF; 
    TWAI.acceptance_filter.amr[3].byte = 0xFF; 

    // Set the acceptance code to 0
    TWAI.acceptance_filter.acr[0].byte = 0x00; 
    TWAI.acceptance_filter.acr[1].byte = 0x00; 
    TWAI.acceptance_filter.acr[2].byte = 0x00; 
    TWAI.acceptance_filter.acr[3].byte = 0x00; 

    // Clear the Reset Mode bit in the mode
    TWAI.mode_reg.rm = 0; 
}