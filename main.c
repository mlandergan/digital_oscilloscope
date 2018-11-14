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
#define ADC_OFFSET 2048 // ticks
#define PIXELS_PER_DIV 20
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define FIFO_SIZE 11        // FIFO capacity is 1 item fewer

// circular fifo for buttons
volatile char fifo[FIFO_SIZE];  // FIFO storage array
volatile int fifo_head = 0; // index of the first item in the FIFO
volatile int fifo_tail = 0; // index one step past the last item

char tmp;
bool risingEdge = false; // TODO: make this be read from button
uint32_t indexVolts = 3;
const char * const gVoltageScaleStr[] = {"100 mV", "200 mV", "500 mV", "1 V"};
float fVoltsPerDiv[] = {0.1, 0.2, 0.5, 1.0}; // Volts TODO: make this be adjusted by button


// ADC variables
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1;  // latest sample index
volatile uint32_t ADC_counts = 0;
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE];           // circular buffer

// Oscilloscope variables
uint32_t triggerIndex;
uint32_t windowWidth = 128;
uint32_t triggerLevel = 2048; // (1.63/3.3) * 4096 = 2048 ADC ticks
uint16_t scopeBuffer[128];

// variables to keep track of time
uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second

// function prototypes
int binaryConversion(int num);
void triggerWindow(void);
void render(char* buttonBuff, char* frequencyBuff, tContext sContext);
void readButtonFifo(void);

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
        readButtonFifo();
        triggerWindow();
        render(&buttonBuff, &frequencyBuff, sContext);
    }
}

// read button FIFO, update risingEdge and fVoltsPerDiv variables
void readButtonFifo(){
    char buttonIDResult;
    int success = fifo_get(&buttonIDResult);
    if(success){
        // update globals
        if(buttonIDResult == '2'){
            risingEdge = 1;
        }
        else if(buttonIDResult == '3'){
            risingEdge = 0;
        }
        else if(buttonIDResult == '7'){// Increment volts per division
             if(indexVolts != 4){
                 indexVolts++;
             }
        }
        else if(buttonIDResult == '8'){
            if(indexVolts != 0){
                indexVolts--;
            }
        }
   }
}

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

    int currIndex;
    int ADCIndex = 0;
    for(currIndex = 0; currIndex < 128; currIndex++){
        ADCIndex = triggerIndex-64 + currIndex;
        scopeBuffer[currIndex] = gADCBuffer[ADCIndex];
    }
}


void render(char* buttonBuff, char* frequencyBuff, tContext sContext){

    GrContextForegroundSet(&sContext, 0xFF);
    int i;
    for (i=0; i<7; i++){
        GrLineDrawV(&sContext, i * ((LCD_HORIZONTAL_MAX/7)) + (LCD_HORIZONTAL_MAX/14), 0, LCD_VERTICAL_MAX);
        GrLineDrawH(&sContext, 0, LCD_HORIZONTAL_MAX, i * (LCD_VERTICAL_MAX/7)+ (LCD_HORIZONTAL_MAX/14));
    }

    GrContextForegroundSet(&sContext, ClrRed); // yellow text

    float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv[indexVolts]);

    int x;
    for(x=0; x < windowWidth; x++){
        float vin = (float)scopeBuffer[x] *(3.3/4096.0);
        int y = LCD_VERTICAL_MAX/2- (int)roundf(fScale * ((int)scopeBuffer[x] - ADC_OFFSET));
        GrLineDrawV(&sContext, x, y, y-1);
    }

    char risingEdgeBuffer[50];
    snprintf(risingEdgeBuffer, sizeof(risingEdgeBuffer), "Rising Edge Bool: %d", risingEdge);

//    char buttonResult[50];
//    snprintf(buttonResult, sizeof(buttonResult), "button ID: %c", buttonIDResult);

    GrContextForegroundSet(&sContext, ClrYellow); // yellow text
    GrStringDraw(&sContext, risingEdgeBuffer, -1, 0, 0, false);
//    GrStringDraw(&sContext, buttonResult, -1, 0, 30, false);

//    GrStringDraw(&scopeBuffer, str, /*length*/ -1, /*x*/ 0, /*y*/ 0, /*opaque*/ false);
//    GrStringDraw(&sContext, buttonBuff, /*length*/ -1, /*x*/ 0, /*y*/ 50, /*opaque*/ false);
//    GrStringDraw(&sContext, frequencyBuff, /*length*/ -1, /*x*/ 0, /*y*/ 80, /*opaque*/ false);
    GrFlush(&sContext); // flush the frame buffer to the LCD

}


