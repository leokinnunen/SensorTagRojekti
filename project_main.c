/*
 * mpu9250_example.c
 *
 *  Created on: 29.10.2016
 *  Author: Teemu Leppanen / UBIComp / University of Oulu
 *
 */
#include <stdio.h>
#include <string.h>
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include "Board.h"
#include "sensors/mpu9250.h"
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

#define STACKSIZE 1024
Char taskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// MPU power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

//STATES
enum state { WAITING=1, DATA_READY };
enum state programState = WAITING;

// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

float ax, ay, az, gx, gy, gz;

Void uartTaskFxn(UArg arg0, UArg arg1) {

    // JTKJ: Tehtävä 4. Lisää UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1
    UART_Handle uart;
    UART_Params uartParams;

    // Initialize serial communication
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600; // 9600 baud rate
    uartParams.dataLength = UART_LEN_8; // 8
    uartParams.parityType = UART_PAR_NONE; // n
    uartParams.stopBits = UART_STOP_ONE; // 1
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
      System_abort("Error opening the UART");
    }
    char debug_msg[64];
    while (1) {

        // JTKJ: Tehtävä 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state
        sprintf(debug_msg,"ax: %.2f\r\nay: %.2f\r\naz: %.2f\r\ngx: %.2f\r\ngy: %.2f\r\ngz: %.2f\r\n",ax, ay, az, gx, gy, gz);
        UART_write(uart, debug_msg, strlen(debug_msg));
        // JTKJ: Tehtävä 4. Lähetä sama merkkijono UARTilla
        // JTKJ: Exercise 4. Send the same sensor data string with UART

        // Just for sanity check for exercise, you can comment this out
        System_printf("uartTask\n");
        System_flush();

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}


Void sensorFxn(UArg arg0, UArg arg1) {


    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
    i2cMPUParams.bitRate = I2C_400kHz;
    // Note the different configuration below
    i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

    // MPU power on
    PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

    // Wait 100ms for the MPU sensor to power up
    Task_sleep(100000 / Clock_tickPeriod);
    System_printf("MPU9250: Power ON\n");
    System_flush();

    // MPU open i2c
    i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
    if (i2cMPU == NULL) {
        System_abort("Error Initializing I2CMPU\n");
    }

    // MPU setup and calibration
    System_printf("MPU9250: Setup and calibration...\n");
    System_flush();

    mpu9250_setup(&i2cMPU);

    System_printf("MPU9250: Setup and calibration OK\n");
    System_flush();

    // Loop forever
    while (1) {

        // MPU ask data
        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        //sprintf(debug_msg,"ax: %.2f\nay: %.2f\naz: %.2f\ngx: %.2f\ngy: %.2f\ngz: %.2f\n",ax, ay, az, gx, gy, gz);
        ////System_flush();
        // Sleep 100ms
        Task_sleep(20000 / Clock_tickPeriod);
    }

    // Program never gets here..
    // MPU close i2c
    // I2C_close(i2cMPU);
    // MPU power off
    // PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_OFF);
}

int main(void) {
    printf("Hello");
    fflush (stdout);
    printf(" World!");
    fflush (stdout);

    Task_Handle task;
    Task_Params taskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;


    Board_initGeneral();
    Board_initI2C();
    Board_initUART();
    // Open MPU power pin
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
        System_abort("Pin open failed!");
    }

    Task_Params_init(&taskParams);
    taskParams.stackSize = STACKSIZE;
    taskParams.stack = &taskStack;
    taskParams.priority=2;
    task = Task_create((Task_FuncPtr)sensorFxn, &taskParams, NULL);
    if (task == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }
    System_printf("Hello World\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
