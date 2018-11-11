/**
 * main.c
 *
 * ECE 3849 Lab 0 Starter Project
 * Gene Bogdanov    10/18/2017
 *
 * This version is using the new hardware for B2017: the EK-TM4C1294XL LaunchPad with BOOSTXL-EDUMKII BoosterPack.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include "buttons.h"
#include "ADC_implementation.h"
#include "limits.h"
#include <math.h>

#define ADC_BUFFER_SIZE 2048                             // size must be a power of 2
#define VIN_RANGE 3.3
#define ADC_BITS 12
#define ADC_OFFSET 380 // volt
#define PIXELS_PER_DIV 20

uint16_t fVoltsPerDiv = 2; // Volts

volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;  // latest sample index
volatile uint32_t ADC_counts = 0;
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];           // circular buffer

// Oscilloscope variables
uint32_t triggerIndex;
uint32_t windowWidth = 2000;
uint32_t triggerLevel = 2048; // (1.63/3.3) * 4096 = 2048 ADC ticks
uint16_t scopeBuffer[ADC_BUFFER_SIZE];

uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
int binary_conversion(int num);
void triggerWindow(void);
void render(char* buttonBuff, char* frequencyBuff, tContext sContext);

int binary_conversion(int num){
    if(num==0){
        return 0;
    }
    else
        return(num%2+10*binary_conversion(num/2));
}

int main(void)
{
    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font


    uint32_t time;  // local copy of gTime
    char str[50];   // string buffer
    char buttonBuff[50];
    char frequencyBuff[50];
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    // Initialize buttons
    ButtonInit();
    ADC1Init();
    IntMasterEnable();

    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font

    while (true) {
        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black

        time = gTime; // read shared global only once
        uint32_t fracSecond = time % 100;
        uint32_t second = (time / 100) % 60;
        uint32_t min = (time /100) / 60;
        triggerWindow();

       // snprintf(str, sizeof(str), "Time = %02u:%02u:%02u", min,second,fracSecond); // convert time to string
        int bin = binary_conversion(gButtons); // convert gButtons into binary
        snprintf(buttonBuff, sizeof(buttonBuff), "Button = %09u",bin);

         // sample -> scopeBuffer
        uint16_t sample = scopeBuffer[8];
        float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv);
        float vin = (float)sample *(3.3/4096.0);
        int y = LCD_VERTICAL_MAX/2- (int)roundf(fScale * ((int)sample - ADC_OFFSET));
        snprintf(frequencyBuff, sizeof(frequencyBuff), "Sample = %f", vin);

        render(&buttonBuff, &frequencyBuff, sContext);
    }
}

void triggerWindow(){
    triggerIndex = gADCBufferIndex - (windowWidth/2);
    uint16_t prevSample = gADCBuffer[triggerIndex];
    uint16_t currSample = INT_MAX;
    int i;
    for(i=triggerIndex; i < (ADC_BUFFER_SIZE/2); i--){

        currSample = gADCBuffer[i];

        if (prevSample > triggerLevel && currSample < triggerLevel){
            int z;
            for(z = (i-(windowWidth/2)); z < (i+(windowWidth/2)); z++){
                scopeBuffer[z] = gADCBuffer[z];
            }
            break;
        }
        prevSample = currSample;
     }
}

void render(char* buttonBuff, char* frequencyBuff, tContext sContext){

    GrContextForegroundSet(&sContext, 0xFF);

    int i;
    for (i=0; i<7; i++){
        GrLineDrawV(&sContext, i * ((LCD_HORIZONTAL_MAX/7)) + (LCD_HORIZONTAL_MAX/14), 0, LCD_VERTICAL_MAX);
        GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX, i * (LCD_VERTICAL_MAX/7)+ (LCD_HORIZONTAL_MAX/14));
    }

    GrContextForegroundSet(&sContext, ClrYellow); // yellow text
//        GrStringDraw(&scopeBuffer, str, /*length*/ -1, /*x*/ 0, /*y*/ 0, /*opaque*/ false);
    GrStringDraw(&sContext, buttonBuff, /*length*/ -1, /*x*/ 0, /*y*/ 50, /*opaque*/ false);
    GrStringDraw(&sContext, frequencyBuff, /*length*/ -1, /*x*/ 0, /*y*/ 80, /*opaque*/ false);
    GrFlush(&sContext); // flush the frame buffer to the LCD

}


