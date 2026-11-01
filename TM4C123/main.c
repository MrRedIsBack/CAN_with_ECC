#include <stdint.h>
#include <stdio.h>
#include "lm4f120h5qr.h"

#define CAN_MODULE0_CLOCK_BIT (1U << 0)
#define PORT_E_BIT            (1U << 4)
#define PORT_F_BIT            (1U << 5)
#define PE4_PE5_MASK          ((1U << 4) | (1U << 5))

#define RED_LED   (1U << 1) // PF1
#define BLUE_LED  (1U << 2) // PF2
#define GREEN_LED (1U << 3) // PF3

volatile uint8_t  rx_data[8];
volatile uint16_t rx_id;
volatile uint8_t  rx_dlc;
volatile uint32_t rx_count = 0;
volatile uint32_t matched_idx = 0xFF;

static void Clock_Init_16MHz_PIOSC(void) {
    SYSCTL_RCC2_R &= ~(1U << 31); // USERCC2 = 0
    SYSCTL_RCC_R  |= (1U << 11);  // BYPASS = 1
    SYSCTL_RCC_R  &= ~(1U << 22); // USESYSDIV = 0
    SYSCTL_RCC_R  &= ~(0x03U << 4);
    SYSCTL_RCC_R  |= (0x01U << 4); // OSCSRC = 1 (16.0 MHz PIOSC)
    SYSCTL_RCC_R  |= (1U << 13);   // PWRDN = 1
    for (volatile int i = 0; i < 10000; i++) {}
}

static const uint32_t bit_table[] = {
    (0x01U) | (0x01U << 6) | (0x0BU << 8) | (0x02U << 12), // 0: 500 kbps @ 16 MHz (BRP=1, 16 Tq)
    (0x00U) | (0x01U << 6) | (0x0BU << 8) | (0x02U << 12), // 1: 1 Mbps @ 16 MHz / 500 kbps @ 8 MHz
    (0x03U) | (0x01U << 6) | (0x0BU << 8) | (0x02U << 12), // 2: 250 kbps @ 16 MHz / 500 kbps @ 32 MHz
    (0x01U) | (0x01U << 6) | (0x05U << 8) | (0x00U << 12), // 3: 1 Mbps @ 16 MHz (BRP=1, 8 Tq)
    (0x04U) | (0x01U << 6) | (0x0FU << 8) | (0x02U << 12), // 4: 500 kbps @ 50 MHz (BRP=4, 20 Tq)
    (0x09U) | (0x01U << 6) | (0x0BU << 8) | (0x02U << 12), // 5: 500 kbps @ 80 MHz (BRP=9, 16 Tq)
    (0x07U) | (0x01U << 6) | (0x0FU << 8) | (0x02U << 12), // 6: 500 kbps @ 80 MHz (BRP=7, 20 Tq)
    (0x04U) | (0x01U << 6) | (0x0BU << 8) | (0x02U << 12)  // 7: 500 kbps @ 40 MHz (BRP=4, 16 Tq)
};
#define NUM_BIT_RATES (sizeof(bit_table) / sizeof(bit_table[0]))

static void Apply_Bit_Rate(uint32_t bit_reg_val) {
    CAN0_CTL_R |= (1U << 0) | (1U << 6) | (1U << 7); // INIT=1, CCE=1, TEST=1
    for (volatile int d = 0; d < 100; d++) {}        // Wait for CAN clock sync
    CAN0_BIT_R  = bit_reg_val;
    CAN0_BRPE_R = 0x00U;
    CAN0_TST_R  = (1U << 3);                         // SILENT = 1
    CAN0_STS_R  = 0x07U;                             // Write 0x7 to LEC to set "No Event"
    CAN0_CTL_R &= ~((1U << 0) | (1U << 6));          // Clear INIT and CCE
    while (CAN0_CTL_R & (1U << 0)) {}                // Wait until hardware exits INIT!
}

int main(void) {
    SYSCTL_RCGCGPIO_R |= (PORT_E_BIT | PORT_F_BIT);
    while ((SYSCTL_PRGPIO_R & (PORT_E_BIT | PORT_F_BIT)) != (PORT_E_BIT | PORT_F_BIT)) {}

    GPIO_PORTF_DIR_R |= (RED_LED | BLUE_LED | GREEN_LED);
    GPIO_PORTF_DEN_R |= (RED_LED | BLUE_LED | GREEN_LED);
    GPIO_PORTF_DATA_R = RED_LED;

    Clock_Init_16MHz_PIOSC();

    SYSCTL_RCGCCAN_R |= CAN_MODULE0_CLOCK_BIT;
    while ((SYSCTL_PRCAN_R & CAN_MODULE0_CLOCK_BIT) == 0) {}

    GPIO_PORTE_AMSEL_R &= ~PE4_PE5_MASK;
    GPIO_PORTE_AFSEL_R |=  PE4_PE5_MASK;
    GPIO_PORTE_PUR_R   |= (1U << 4);
    GPIO_PORTE_DEN_R   |=  PE4_PE5_MASK;
    GPIO_PORTE_PCTL_R  &= ~((0xFU << 16) | (0xFU << 20));
    GPIO_PORTE_PCTL_R  |=  ((8U << 16) | (8U << 20));

    CAN0_CTL_R |= (1U << 0) | (1U << 6);
    for (volatile int d = 0; d < 100; d++) {}

    while (CAN0_IF1CRQ_R & (1U << 15)) {}
    CAN0_IF1CMSK_R = (1U << 7) | (1U << 6) | (1U << 5) | (1U << 4);
    CAN0_IF1MSK1_R = 0x0000U;
    CAN0_IF1MSK2_R = 0x0000U;
    CAN0_IF1ARB1_R = 0x0000U;
    CAN0_IF1ARB2_R = (1U << 15);
    CAN0_IF1MCTL_R = (1U << 12) | (1U << 7);
    CAN0_IF1CRQ_R  = 1U;
    while (CAN0_IF1CRQ_R & (1U << 15)) {}

    uint32_t idx = 0;
    Apply_Bit_Rate(bit_table[idx]);

    while (1) {
        uint32_t sts = CAN0_STS_R;
        uint32_t lec = sts & 0x07U;

        if ((CAN0_NWDA1_R & (1U << 0)) || (sts & (1U << 4))) {
            while (CAN0_IF2CRQ_R & (1U << 15)) {}
            CAN0_IF2CMSK_R = (1U << 5) | (1U << 4) | (1U << 3) | (1U << 2) | (1U << 1) | (1U << 0);
            CAN0_IF2CRQ_R  = 1U;
            while (CAN0_IF2CRQ_R & (1U << 15)) {}

            rx_id       = (CAN0_IF2ARB2_R >> 2) & 0x07FFU;
            rx_dlc      = CAN0_IF2MCTL_R & 0x0FU;
            rx_data[0]  = (uint8_t)(CAN0_IF2DA1_R & 0xFFU);
            rx_count++;
            matched_idx = idx;

            GPIO_PORTF_DATA_R = GREEN_LED; // SOLID GREEN = RECEIVED!
        } else if (lec != 0 && lec != 7 && rx_count == 0) {
            // Alternate Yellow and Blue on each 1-second pulse so you can SEE it stepping!
            idx = (idx + 1) % NUM_BIT_RATES;
            if (idx & 1) {
                GPIO_PORTF_DATA_R = BLUE_LED;
            } else {
                GPIO_PORTF_DATA_R = (RED_LED | GREEN_LED); // Yellow
            }
            for (volatile int d = 0; d < 200000; d++) {} // Wait ~30ms for current frame to finish
            Apply_Bit_Rate(bit_table[idx]);
        }
    }
}