#include "lm4f120h5qr.h"

#define CAN_MODULE0_CLOCK_BIT (1U)
#define PORT_B_BIT (1U << 1)
#define PB4_PB5_MASK ((1U << 4) | (1U << 5))

int main () {
	
		SYSCTL_RCGCCAN_R |= CAN_MODULE0_CLOCK_BIT;
		
		// Wait for the CAN module 0 to be ready
		while ((SYSCTL_PRCAN_R & CAN_MODULE0_CLOCK_BIT) == 0) {}

		SYSCTL_RCGCGPIO_R |= PORT_B_BIT;
		while ((SYSCTL_PRGPIO_R & PORT_B_BIT) == 0) {}
			
		GPIO_PORTB_AMSEL_R &= ~PB4_PB5_MASK; // 0 = Disable analog on PB4 & PB5
		GPIO_PORTB_AFSEL_R |=  PB4_PB5_MASK; // 1 = Enable alternate function on PB4 & PB5
		GPIO_PORTB_DEN_R   |=  PB4_PB5_MASK; // 1 = Enable digital I/O on PB4 & PB5
		GPIO_PORTB_PCTL_R &= ~((0xFU << 16) | (0xFU << 20));
		GPIO_PORTB_PCTL_R |= ((8U << 16) | (8U << 20));
			
		// CAN config
		CAN0_CTL_R |= ((1U) | (1U << 6)); // Bit 0 - Initialisation Started Bit 6 - Write accesses to the CANBIT register are allowed
		CAN0_BIT_R = ((0x04U) | (0x2U << 6) | (0xEU << 8) | (0x3U << 12));
		CAN0_CTL_R &= ~((1U) | (1U << 6)); // Bit 0 - Normal operation Bit 6 - Write accesses to the CANBIT register are not allowed
	
	return 0;
}