// TM4C123 (Tiva C LaunchPad) - bare-metal CAN0 receiver on PE4/PE5
//
// Wiring (outer left header J1, counting down from +3.3V at the top):
//   pin 1  +3.3V -> module 3.3V
//   pin 5  PE4   -> module RX   (CAN0RX)
//   pin 6  PE5   -> module TX   (CAN0TX)
//   GND          -> module GND, plus one wire to an ESP32 GND pin
//
// 125 kbit/s, system clock = 50 MHz (set by SystemInit in startup_TM4C123.s)
// Receives standard ID 0x2BC into message object 1
//
// LEDs:
//   Blue  (PF2) blinks  -> program is running
//   Green (PF3) toggles -> received ID 0x2BC with data[0] == 0x05
//
// Debug values (watch these in Keil, they work without debug symbols):
//   *(unsigned int *)0x20006000  number of frames received
//   *(unsigned int *)0x20006004  ID of the last frame received   (0x2BC expected)
//   *(unsigned int *)0x20006008  data[0] of the last frame      (0x05 expected)
//   *(unsigned int *)0x40040004  CAN0_STS (bit 4 = RXOK, bits 2:0 = last error code)
//   *(unsigned int *)0x40040008  CAN0_ERR (bits 14:8 = receive error count)

#include "lm4f120h5qr.h"
#include <stdint.h>

#define CAN0_CLK      (1U)
#define PORTE_CLK     (1U << 4)
#define PORTF_CLK     (1U << 5)
#define PE4_PE5_MASK  ((1U << 4) | (1U << 5))
#define IF_BUSY       (1U << 15)

#define LED_BLUE      (1U << 2)
#define LED_GREEN     (1U << 3)

#define DBG           ((volatile uint32_t *)0x20006000)

volatile uint32_t rx_id, rx_dlc, rx_count;
volatile uint8_t  rx_data[8];

int main(void) {
    // --- Clocks: CAN0, Port E, Port F ---
    SYSCTL_RCGCCAN_R  |= CAN0_CLK;
    SYSCTL_RCGCGPIO_R |= PORTE_CLK | PORTF_CLK;
    while ((SYSCTL_PRCAN_R & CAN0_CLK) == 0) {}
    while ((SYSCTL_PRGPIO_R & (PORTE_CLK | PORTF_CLK)) != (PORTE_CLK | PORTF_CLK)) {}

    // --- Debug values ---
    DBG[0] = 0;
    DBG[1] = 0;
    DBG[2] = 0;

    // --- LEDs on PF2 (blue) and PF3 (green) ---
    GPIO_PORTF_DIR_R |= LED_BLUE | LED_GREEN;
    GPIO_PORTF_DEN_R |= LED_BLUE | LED_GREEN;

    // --- PE4 = CAN0RX, PE5 = CAN0TX (alternate function 8) ---
    GPIO_PORTE_AMSEL_R &= ~PE4_PE5_MASK;
    GPIO_PORTE_AFSEL_R |=  PE4_PE5_MASK;
    GPIO_PORTE_DEN_R   |=  PE4_PE5_MASK;
    GPIO_PORTE_PCTL_R   = (GPIO_PORTE_PCTL_R & ~((0xFU << 16) | (0xFU << 20)))
                        | (8U << 16) | (8U << 20);

    // --- Enter init mode, set bit timing ---
    CAN0_CTL_R |= (1U << 0) | (1U << 6);      // INIT + CCE
    CAN0_BIT_R  = 0x2F93;                     // 50 MHz: BRP=20, SJW=3, TSEG1=16, TSEG2=3 -> 125 kbit/s, 85% SP

    // --- Invalidate all 32 message objects ---
    for (uint32_t n = 1; n <= 32; n++) {
        while (CAN0_IF1CRQ_R & IF_BUSY) {}
        CAN0_IF1CMSK_R = (1U << 7) | (1U << 5) | (1U << 4); // WRNRD, ARB, CONTROL
        CAN0_IF1ARB1_R = 0;
        CAN0_IF1ARB2_R = 0;                   // MSGVAL = 0
        CAN0_IF1MCTL_R = 0;
        CAN0_IF1CRQ_R  = n;
    }

    // --- Message object 1: receive standard ID 0x2BC ---
    while (CAN0_IF1CRQ_R & IF_BUSY) {}
    CAN0_IF1CMSK_R = (1U << 7) | (1U << 6) | (1U << 5) | (1U << 4); // WRNRD, MASK, ARB, CONTROL
    CAN0_IF1MSK1_R = 0;
    CAN0_IF1MSK2_R = (0x7FFU << 2);           // compare all 11 ID bits
    CAN0_IF1ARB1_R = 0;
    CAN0_IF1ARB2_R = (1U << 15) | (0x2BCU << 2); // MSGVAL=1, standard ID, DIR=0 (receive)
    CAN0_IF1MCTL_R = (1U << 12) | (1U << 7) | 8; // UMASK, EOB, DLC=8
    CAN0_IF1CRQ_R  = 1;
    while (CAN0_IF1CRQ_R & IF_BUSY) {}

    // --- Leave init mode: join the bus ---
    CAN0_CTL_R &= ~((1U << 0) | (1U << 6));

    uint32_t hb = 0;

    while (1) {
        // Heartbeat: blue LED blinks while the program runs
        if (++hb >= 2000000) {
            hb = 0;
            GPIO_PORTF_DATA_R ^= LED_BLUE;
        }

        if (CAN0_NWDA1_R & (1U << 0)) {       // object 1 has new data
            // Read ARB, CONTROL, clear INTPND, clear NEWDAT, data A + B
            CAN0_IF2CMSK_R = (1U << 5) | (1U << 4) | (1U << 3) | (1U << 2) | (1U << 1) | (1U << 0);
            CAN0_IF2CRQ_R  = 1;
            while (CAN0_IF2CRQ_R & IF_BUSY) {}

            rx_id  = (CAN0_IF2ARB2_R >> 2) & 0x7FF;
            rx_dlc =  CAN0_IF2MCTL_R & 0xF;

            uint32_t a1 = CAN0_IF2DA1_R, a2 = CAN0_IF2DA2_R;
            uint32_t b1 = CAN0_IF2DB1_R, b2 = CAN0_IF2DB2_R;
            rx_data[0] = a1 & 0xFF;  rx_data[1] = a1 >> 8;
            rx_data[2] = a2 & 0xFF;  rx_data[3] = a2 >> 8;
            rx_data[4] = b1 & 0xFF;  rx_data[5] = b1 >> 8;
            rx_data[6] = b2 & 0xFF;  rx_data[7] = b2 >> 8;

            rx_count++;

            DBG[0] = rx_count;
            DBG[1] = rx_id;
            DBG[2] = rx_data[0];

            if (rx_id == 0x2BC && rx_data[0] == 0x05) {
                GPIO_PORTF_DATA_R ^= LED_GREEN;
            }
        }
    }
}