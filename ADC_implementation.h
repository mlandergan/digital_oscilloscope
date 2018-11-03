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

// initialize ADC1
void ADC1Init(void);

void ADC_ISR(void);


#endif /* ADC_IMPLEMENTATION_H_ */
