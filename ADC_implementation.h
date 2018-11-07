/*
 * ADC_implementation.h
 *
 *  Created on: Oct 31, 2018
 *      Author: Mark Landergan
 */

#ifndef ADC_IMPLEMENTATION_H_
#define ADC_IMPLEMENTATION_H_

#include "sysctl_pll.h"
#include "driverlib/adc.h"
#include "inc/tm4c1294ncpdt.h"
#include <stdint.h>
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"

// ADC ISR
#define ADC_BUFFER_SIZE 2048                             // size must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
extern volatile int32_t gADCBufferIndex;  // latest sample index
extern volatile uint16_t gADCBuffer;           // circular buffer
volatile uint32_t gADCErrors;                       // number of missed ADC deadlines
#define ADC_SAMPLING_RATE 1000000   // [samples/sec] desired ADC sampling rate
#define CRYSTAL_FREQUENCY 25000000  // [Hz] crystal oscillator frequency used to calculate clock rates
extern volatile uint32_t ADC_counts;

// initialize ADC1
void ADC1Init(void);

void ADC_ISR(void);


#endif /* ADC_IMPLEMENTATION_H_ */
