/*
 * ADC_implementation.c
 *
 *  Created on: Oct 31, 2018
 *      Author: Mark Landergan
 */
#include "ADC_implementation.h"

void ADC1Init(void)
{
    // initialize ADC1 sampling sequence
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIOPinTypeADC(GPIO_PORTB_BASE, GPIO_PIN_4); // GPIO setup for analog input AIN3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1); // initialize ADC peripherals

    // ADC clock
    uint32_t pll_frequency = SysCtlFrequencyGet(CRYSTAL_FREQUENCY);
    uint32_t pll_divisor = (pll_frequency - 1) / (16 * ADC_SAMPLING_RATE) + 1; //round up
    ADCClockConfigSet(ADC0_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    ADCSequenceDisable(ADC1_BASE, 0);      // choose ADC1 sequence 0; disable before configuring
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0);             // specify the "Always" trigger
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH3 | ADC_CTL_IE | ADC_CTL_END);                // in the 0th step, sample channel 3 (AIN3)
                                                                           // enable interrupt, and make it the end of sequence
    ADCSequenceEnable(ADC1_BASE, 0);                                       // enable the sequence.  it is now sampling
    ADCIntEnable(ADC1_BASE, 0);                                            // enable sequence 0 interrupt in the ADC1 peripheral
    IntPrioritySet(INT_ADC1SS0, 0);                                        // set ADC1 sequence 0 interrupt priority
    IntEnable(INT_ADC1SS0);                                                // enable ADC1 sequence 0 interrupt in int. controller
}


void ADC_ISR(void)
{
    ADCIntClear(ADC1_BASE, 0); // clear ADC1 sequence0 interrupt flag in the ADCISC register
    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
        gADCErrors++;                   // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0;   // clear overflow condition
    }

    ADC_counts = ADC1_SSFIFO0_R; // read ADC1 FIFO register direcly 

    gADCBuffer[
               gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
               ] = ADC_counts;               // Put current ADC_counts into the gADCBuffer
}

