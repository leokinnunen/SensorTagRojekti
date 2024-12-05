/*
 * buttonwtf.c
 *
 *  Created on: Dec 4, 2024
 *      Author: linuxlite

#include <stdio.h>

//XDCtools files
#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include "Board.h"
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
// RTOS global variables for handling the pins
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

// Pin configurations, with separate configuration for each pin
// The constant BOARD_BUTTON_0 corresponds to one of the buttons
PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};

// The constant Board_LED0 corresponds to one of the LEDs
PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};

// Button press interrupt handler function
// Use global variables not arguments!!!!
void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

   // Toggle the state of the LED pin using negation
   uint_t pinValue = PIN_getOutputValue(Board_LED0);
   pinValue = !pinValue;
   PIN_setOutputValue(ledHandle, Board_LED0, pinValue);
}

int main(void) {

   Board_initGeneral();

   // Enable the pins for use in the program
   buttonHandle = PIN_open(&buttonState, buttonConfig);
   if(!buttonHandle) {
      System_abort("Error initializing button pins\n");
   }
   ledHandle = PIN_open(&ledState, ledConfig);
   if(!ledHandle) {
      System_abort("Error initializing LED pins\n");
   }

   // Set the button pin’s interrupt handler to function buttonFxn
   if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
      System_abort("Error registering button callback function");
   }

   BIOS_start();

   return (0);
}

*/

