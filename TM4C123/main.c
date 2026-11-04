// TM4C123 - measure the CAN bit length the Tiva actually sees on PE4
// (temporary test, replaces main.c)
//
// Measures every LOW and HIGH pulse on PE4 in system clock cycles (50 MHz).
// At 500 kbit/s, one bit = 2 us = 100 cycles at 50 MHz.
//
// Watch these in Keil (no debug symbols needed):
//   *(unsigned int *)0x20006000  shortest LOW pulse  (cycles)
//   *(unsigned int *)0x20006004  shortest HIGH pulse (cycles)
//   *(unsigned int *)0x20006008  pulses shorter than 80 cycles   (glitches / too fast)
//   *(unsigned int *)0x2000600C  pulses of 80-120 cycles         (one bit at 500 kbit/s)
//   *(unsigned int *)0x20006010  total pulses counted
//   *(unsigned int *)0x20006014  system clock check: RCC register (0x01DE0542 = 50 MHz)

#include "lm4f120h5qr.h"
#include <stdint.h>

#define PE4     (1U << 4)
#define PE5     (1U << 5)
#define RESULT  ((volatile uint32_t *)0x20006000)

static void record(uint32_t width) {
    if (width < 80)        RESULT[2]++;
    else if (width <= 120) RESULT[3]++;
    RESULT[4]++;
}

int main(void) {
    SYSCTL_RCGCGPIO_R |= (1U << 4);                     // Port E
    while ((SYSCTL_PRGPIO_R & (1U << 4)) == 0) {}

    // PE4 = plain input, PE5 = output held high (transceiver stays recessive)
    GPIO_PORTE_AFSEL_R &= ~(PE4 | PE5);
    GPIO_PORTE_AMSEL_R &= ~(PE4 | PE5);
    GPIO_PORTE_DIR_R   &= ~PE4;
    GPIO_PORTE_DIR_R   |=  PE5;
    GPIO_PORTE_DEN_R   |=  PE4 | PE5;
    GPIO_PORTE_DATA_R  |=  PE5;

    // SysTick as a free-running 24-bit down-counter at the system clock
    NVIC_ST_CTRL_R    = 0;
    NVIC_ST_RELOAD_R  = 0x00FFFFFF;
    NVIC_ST_CURRENT_R = 0;
    NVIC_ST_CTRL_R    = 0x5;                            // enable, system clock

    RESULT[0] = 0xFFFFFFFF;
    RESULT[1] = 0xFFFFFFFF;
    RESULT[2] = 0;
    RESULT[3] = 0;
    RESULT[4] = 0;
    RESULT[5] = SYSCTL_RCC_R;

    while (1) {
        while (GPIO_PORTE_DATA_R & PE4) {}              // wait for falling edge
        uint32_t t0 = NVIC_ST_CURRENT_R;
        while ((GPIO_PORTE_DATA_R & PE4) == 0) {}       // wait for rising edge
        uint32_t t1 = NVIC_ST_CURRENT_R;
        while (GPIO_PORTE_DATA_R & PE4) {}              // wait for next falling edge
        uint32_t t2 = NVIC_ST_CURRENT_R;

        uint32_t low  = (t0 - t1) & 0x00FFFFFF;         // SysTick counts down
        uint32_t high = (t1 - t2) & 0x00FFFFFF;

        if (low  < RESULT[0]) RESULT[0] = low;
        if (high < RESULT[1]) RESULT[1] = high;
        record(low);
        if (high < 100000) record(high);                // ignore long idle gaps
    }
}