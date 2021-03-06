/**
 * main.c
 *
 * ECE 3849 Lab 0 Starter Project
 * Gene Bogdanov    10/18/2017
 *
 * Modified by Mark Landergan and Sean O'neil
 * 11/18/2018 
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
#define ADC_OFFSET 2048 // ticks
#define PIXELS_PER_DIV 20
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define FIFO_SIZE 11        // FIFO capacity is 1 item fewer

// circular fifo for buttons
volatile char fifo[FIFO_SIZE];  // FIFO storage array
volatile int fifo_head = 0; // index of the first item in the FIFO
volatile int fifo_tail = 0; // index one step past the last item

// display variables
bool risingEdge = false;
uint32_t indexVolts = 3;
const char * const gVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", "1 V"};
float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1.0};


// ADC variables
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;  // latest sample index
volatile uint32_t ADC_counts = 0;
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];           // circular buffer

// Oscilloscope variables
uint32_t triggerIndex;
uint32_t windowWidth = 128;
uint32_t triggerLevel = 2048; // (1.65/3.3) * 4096 = 2048 ADC ticks
uint16_t scopeBuffer[128];

// variables to keep track of time
uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second

// CPU load counters
uint32_t count_unloaded = 0;
uint32_t count_loaded = 0;
float cpu_load = 0.0;

int binaryConversion(int num);
void triggerWindow(void);
void render(tContext sContext);
void readButtonFifo(void);
uint32_t cpu_load_count(void);
void drawEdge(tContext sContext);


int binaryConversion(int num){
    if(num==0){
        return 0;
    }
    else
        return(num%2+10*binaryConversion(num/2));
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

    // initialize timer 3 in one-shot mode for polled timing
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
    TimerDisable(TIMER3_BASE, TIMER_BOTH);
    TimerConfigure(TIMER3_BASE, TIMER_CFG_ONE_SHOT);
    TimerLoadSet(TIMER3_BASE, TIMER_A, gSystemClock/100-1); // 10 msec interval

    count_unloaded = cpu_load_count();

    char buttonBuff[50];
    char frequencyBuff[50];
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    // Initialize buttons
    ButtonInit();
    ADC1Init();
    IntMasterEnable();

    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font

    // main loop
    while (true) {
        count_loaded = cpu_load_count();
        cpu_load = (1.0f - (float)count_loaded/count_unloaded)*100.0; // compute CPU load
        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        readButtonFifo();
        triggerWindow();
        render(sContext);
    }
}

// read button FIFO, update risingEdge and fVoltsPerDiv variables
void readButtonFifo(){
    char buttonIDResult;
    int success = fifo_get(&buttonIDResult);
    if(success){
        // check for different button IDs and set the appropriate global variables
        if(buttonIDResult == '2'){
            risingEdge = 1;
        }
        else if(buttonIDResult == '3'){
            risingEdge = 0;
        }
        else if(buttonIDResult == '8'){// Increment volts per division
             if(indexVolts != 3){
                 indexVolts++;
             }
        }
        else if(buttonIDResult == '7'){
            if(indexVolts != 0){
                indexVolts--;
            }
        }
   }
}

// Algorithm to find the samples that should be displayed in the trigger window and save them to the scopeBuffer array
void triggerWindow(){
    triggerIndex = ADC_BUFFER_WRAP(gADCBufferIndex - (windowWidth/2));
    uint16_t prevSample = gADCBuffer[triggerIndex];
    uint16_t currSample = INT_MAX;
    int counts = 0;

    // search for trigger
    while((gADCBuffer[triggerIndex] != triggerLevel)){
        currSample = gADCBuffer[triggerIndex];
        if(risingEdge){
            if(counts == (ADC_BUFFER_SIZE/2) || (prevSample > triggerLevel && currSample <= triggerLevel)){
                break;
            }
        }
        else{
            if(counts == (ADC_BUFFER_SIZE/2) || (prevSample < triggerLevel && currSample >= triggerLevel)){
                break;
            }
        }
        prevSample = currSample;
        triggerIndex = ADC_BUFFER_WRAP(triggerIndex--);
        counts += 1;
    }

    // Populate scopeBuffer[] with samples from gADCBuffer within half a window width of the trigger sample in either direction
    int currIndex;
    int ADCIndex = 0;
    for(currIndex = 0; currIndex < windowWidth; currIndex++){ 
        ADCIndex = triggerIndex - (windowWidth/2) + currIndex;
        scopeBuffer[currIndex] = gADCBuffer[ADCIndex];
    }
}


void render(tContext sContext){

    // Drawing gridlines
    GrContextForegroundSet(&sContext, 0xFF);
    int i;
    for (i=0; i<7; i++){
        GrLineDrawV(&sContext, i * ((LCD_HORIZONTAL_MAX/7)) + (LCD_HORIZONTAL_MAX/14), 0, LCD_VERTICAL_MAX);
        GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX, i * (LCD_VERTICAL_MAX/7)+ (LCD_HORIZONTAL_MAX/14));
    }

    // Draw waveform
    GrContextForegroundSet(&sContext, ClrRed); // yellow text
    float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[indexVolts]);

    int x;
    for(x=0; x < windowWidth; x++){
        float vin = (float)scopeBuffer[x] *(3.3/4096.0);
        int y = LCD_VERTICAL_MAX/2- (int)roundf(fScale * ((int)scopeBuffer[x] - ADC_OFFSET));
        GrLineDrawV(&sContext, x, y, y-1);
    }

    // Format text readout values into character buffers
    char timeScaleBuff[10];
    snprintf(timeScaleBuff, sizeof(timeScaleBuff), " %d us", PIXELS_PER_DIV);

    char cpuLoadDisplayBuff[50];
    snprintf(cpuLoadDisplayBuff, sizeof(cpuLoadDisplayBuff), "CPU load = %01f%%", cpu_load);

    char voltageDisplayBuff[50];
    snprintf(voltageDisplayBuff, sizeof(voltageDisplayBuff), "%s", gVoltageScaleStr[indexVolts]);

    GrLineDrawV(&sContext, i * ((LCD_HORIZONTAL_MAX/7)) + (LCD_HORIZONTAL_MAX/14), 0, LCD_VERTICAL_MAX);
    GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX, i * (LCD_VERTICAL_MAX/7)+ (LCD_HORIZONTAL_MAX/14));

    GrContextForegroundSet(&sContext, ClrYellow); // yellow text

    // Draw text
    GrStringDraw(&sContext, cpuLoadDisplayBuff, /*length*/ -1, /*x*/ 0, /*y*/ 120, /*opaque*/ false);
    GrStringDraw(&sContext, timeScaleBuff, /*length*/ -1, /*x*/ 0, /*y*/ 0, /*opaque*/ false);
    GrStringDraw(&sContext, voltageDisplayBuff, /*length*/ -1, /*x*/ 50, /*y*/ 0, /*opaque*/ false);

    // Draw the rising/falling edge
    drawEdge(sContext);

    GrFlush(&sContext); // flush the frame buffer to the LCD

}

// function to draw either rising or falling edge
void drawEdge(tContext sContext){
    GrLineDrawV(&sContext, 100, 10, 0);
    if(risingEdge){
        GrLineDrawH(&sContext, 90, 100, 10);
        GrLineDrawH(&sContext, 100, 110, 0);
        GrLineDraw(&sContext, 97, 8, 100, 3);
        GrLineDraw(&sContext, 103, 8,100, 3);
    }
    else{
        GrLineDrawH(&sContext, 90, 100, 0);
        GrLineDrawH(&sContext, 100, 110, 10);
        GrLineDraw(&sContext, 97, 2, 100, 7);
        GrLineDraw(&sContext, 103, 2, 100, 7);
    }

}

// function to calculate CPU load count
uint32_t cpu_load_count(void)
{
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
    while (!(TimerIntStatus(TIMER3_BASE, false) & TIMER_TIMA_TIMEOUT))
        i++;
    return i;
}


