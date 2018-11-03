/*
 * ADC_implementation.c
 *
 *  Created on: Oct 31, 2018
 *      Author: Mark Landergan
 */
#include "ADC_implementation.h"

// ADC ISR
#define ADC_BUFFER_SIZE 2048                             // size must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;  // latest sample index
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];           // circular buffer
volatile uint32_t gADCErrors;                       // number of missed ADC deadlines
#define ADC_SAMPLING_RATE 1000000   // [samples/sec] desired ADC sampling rate
#define CRYSTAL_FREQUENCY 25000000  // [Hz] crystal oscillator frequency used to calculate clock rates
volatile uint32_t ADC_counts = 0;

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

/*
     ADCProcessorTrigger(ADC1_BASE, 0);          // trigger the ADC sample sequence from circuit
     while(!ADCIntStatus(ADC1_BASE, 0, false));  // wait until the sample sequence has completed
     ADCSequenceDataGet(ADC1_BASE, 0, ADC_counts);// retrieve ADC counts data
 */

void ADC_ISR(void)
{
    ADCIntClear(ADC1_BASE, 0); // clear ADC1 sequence0 interrupt flag in the ADCISC register
    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
        gADCErrors++;                   // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0;   // clear overflow condition
    }

    ADCSequenceDataGet(ADC1_BASE, 0, &ADC_counts);// retrieve ADC counts data

    gADCBuffer[
               gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
               ] = ADC_counts;               // read sample from the ADC1 sequence 0 FIFO
}

