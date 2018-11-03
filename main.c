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

uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
unsigned char getBits(size_t const size, void const * const ptr); // print out binary representation of values

//assumes little endian
unsigned char getBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;

    int i, j;

    for (i=size-1;i>=0;i--)
    {
        for (j=7;j>=0;j--)
        {
            byte = byte | ((b[i] >> j) & 1);
        }
    }
    return byte;
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
    char buttonBuff[50];   // string buffer
    char binary[50];
    unsigned char byte;

    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    ButtonInit();
    IntMasterEnable();

    while (true) {
        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        time = gTime; // read shared global only once
        uint32_t fracSecond = time%100; // get the remainder of 100 for fraction of second
        uint32_t second = (time/100)%60; // get the remainder of 60 for seconds
        uint32_t min = (time/100)/60;
        snprintf(str, sizeof(str), "Time = %02u:%02u:%02u", min,second,fracSecond); // convert time to string
        snprintf(buttonBuff, sizeof(buttonBuff), "Button = %09u", gButtons); // display the button states in binary
        GrContextForegroundSet(&sContext, ClrYellow); // yellow text
        GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 0, /*y*/ 0, /*opaque*/ false);
        GrStringDraw(&sContext, buttonBuff, /*length*/ -1, /*x*/ 0, /*y*/ 100, /*opaque*/ false);

        //unsigned char byte;
        byte = getBits(sizeof(buttonBuff), &buttonBuff);
        snprintf(binary, sizeof(binary), "%c", byte); // display the button states in binary
        GrStringDraw(&sContext, binary, /*length*/ -1, /*x*/ 50, /*y*/ 50, /*opaque*/ false);

        GrFlush(&sContext); // flush the frame buffer to the LCD
    }
}
