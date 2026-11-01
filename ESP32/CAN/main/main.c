#include <stdio.h>
#include <inttypes.h>

#include "soc/twai_struct.h"
#include "soc/dport_reg.h"
#include "soc/gpio_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CAN_CLK_EN_BIT (1U << 19)
#define CAN_RST_BIT    (1U << 19)

#define GPIO_TX_PIN 21
#define GPIO_RX_PIN 22
#define GPIO_TX_BIT (1U << GPIO_TX_PIN)
#define GPIO_RX_BIT (1U << GPIO_RX_PIN)

void app_main(void) {
    // 1. Enable and reset TWAI peripheral using safe DPORT macros
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, CAN_CLK_EN_BIT);
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, CAN_RST_BIT);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, CAN_RST_BIT);

    // 2. Bare-metal GPIO21 (TX) and GPIO22 (RX) configuration
    PIN_FUNC_SELECT(IO_MUX_GPIO21_REG, PIN_FUNC_GPIO);
    PIN_FUNC_SELECT(IO_MUX_GPIO22_REG, PIN_FUNC_GPIO);
    PIN_INPUT_ENABLE(IO_MUX_GPIO22_REG);
    REG_SET_BIT(IO_MUX_GPIO22_REG, FUN_PU); // Enable internal pull-up on RX

    GPIO.enable_w1tc = GPIO_RX_BIT; // GPIO22 as input
    GPIO.enable_w1ts = GPIO_TX_BIT; // GPIO21 as output

    // Route TX through GPIO matrix and force Output Enable from GPIO.enable register
    GPIO.pin[GPIO_TX_PIN].pad_driver = 0; // Push-pull output
    GPIO.func_out_sel_cfg[GPIO_TX_PIN].func_sel = TWAI_TX_IDX;
    GPIO.func_out_sel_cfg[GPIO_TX_PIN].inv_sel = 0;
    GPIO.func_out_sel_cfg[GPIO_TX_PIN].oen_sel = 1; // Use GPIO.enable_w1ts to enable output

    // Route RX through GPIO matrix
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].func_sel = GPIO_RX_PIN;
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].sig_in_inv = 0;
    GPIO.func_in_sel_cfg[TWAI_RX_IDX].sig_in_sel = 1;

    // 3. Enter Reset Mode (rm = 1) + Single Acceptance Filter Mode (afm = 1)
    TWAI.mode_reg.val = (1U << 0) | (1U << 3);

    // 4. Pelican/Extended register layout (cm = 1) & disable clock out (co = 1)
    TWAI.clock_divider_reg.val = (1U << 7) | (1U << 3);

    // Direct .val writes so ESP-IDF v6.1 bitfield shifts cannot corrupt BTR0 or BTR1!
    TWAI.bus_timing_0_reg.val = 0x43; // SJW=1, BRP=3 -> 10 MHz Tq clock
    TWAI.bus_timing_1_reg.val = 0x2F; // SAM=0, TSEG2=2 (3 Tq), TSEG1=15 (16 Tq) -> 20 Tq total = 500 kbps

    // 6. Acceptance filter: accept all messages
    for (int i = 0; i < 4; i++) {
        TWAI.acceptance_filter.acr[i].val = 0x00;
        TWAI.acceptance_filter.amr[i].val = 0xFF;
    }

    // 7. Clear any pending interrupts and exit Reset Mode (rm = 0, afm = 1)
    (void)TWAI.interrupt_reg.val;
    TWAI.mode_reg.val = (1U << 2) | (1U << 3);

    while (1) {
        if (TWAI.status_reg.tbs == 0) {
            TWAI.command_reg.val = (1U << 4); // Abort stuck transmission if any (AT = 1)
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // Standard Data Frame, ID = 0x2BC, DLC = 1, Data[0] = 0x05
        TWAI.tx_rx_buffer[0].val = 0x01; // FF=0 (Std), RTR=0 (Data), DLC=1
        TWAI.tx_rx_buffer[1].val = 0x57; // ID[10:3] of 0x2BC (0x2BC >> 3 = 0x57)
        TWAI.tx_rx_buffer[2].val = 0x80; // ID[2:0] of 0x2BC ((0x2BC & 0x07) << 5 = 0x80)
        TWAI.tx_rx_buffer[3].val = 0x05; // Data byte 0

        TWAI.command_reg.val = (1U << 0); // Normal Transmission Request (TR = 1)

        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("BTR0: 0x%02" PRIx32 " | Status: 0x%02" PRIx32 " | TXERR: %" PRIu32 " | ECC: 0x%02" PRIx32 "\n",
               TWAI.bus_timing_0_reg.val, TWAI.status_reg.val, TWAI.tx_error_counter_reg.val, TWAI.error_code_capture_reg.val);
    }
}