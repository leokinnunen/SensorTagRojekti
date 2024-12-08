/*
 * LEO KINNUNEN TKJ 2024
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
#include "buzzer.h"
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>

#define STACKSIZE 1024
Char taskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];
Char buttonTaskStack[STACKSIZE];
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};


//pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static UART_Handle uart;
static UART_Params uartParams;
//STATES
enum state { SENDING=1, RECEIVING };
enum state programState = SENDING;

enum { BUTTON_NOT_PRESSED = 0, BUTTON_PRESSED};
uint_t button = BUTTON_NOT_PRESSED;
// MPU power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};


PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // The configuration table is always terminated with this constant
};

// MPU uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

float ax, ay, az, gx, gy, gz;
//Moving averages
float ax_ma, az_ma;

enum {DOT, DASH, SPACE, NOTHING};
uint8_t sound = NOTHING;


void buzzer_delay(uint32_t milliseconds) {
    volatile uint32_t i, j;
    for (i = 0; i < milliseconds; i++) {
        for (j = 0; j < 4000; j++) {
            //just waste time
        }
    }
}



void buzzerFxn(char message)
{

        if(message=='.')
        {
            buzzerOpen(hBuzzer);
            buzzerSetFrequency(440);
            buzzer_delay(50);
            buzzerClose();
            buzzer_delay(50);
        }
        else if(message=='-')
        {
            buzzerOpen(hBuzzer);
            buzzerSetFrequency(440);
            buzzer_delay(200);
            buzzerClose();
            buzzer_delay(50);
        }


}


void NOTE(uint16_t freq, uint32_t milliseconds)
{
    buzzerOpen(hBuzzer);
    buzzerSetFrequency(freq);
    buzzer_delay(milliseconds);
    buzzerClose();
}

void dashnote()
{
    NOTE(520, 50);
    NOTE(493, 50);
    NOTE(440, 50);
    NOTE(392, 100);
}

void dotnote()
{
    NOTE(520, 50);
    NOTE(784, 50);
    NOTE(738, 50);
    NOTE(656, 50);
}

void spacenote()
{
    NOTE(392, 100);
    NOTE(328, 50);
    NOTE(293, 50);
    NOTE(260, 50);
}

void buzzerSolo()
{
    //Finlandia hymni
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 500);
    NOTE(520, 1000);
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 500);
    NOTE(392, 750);
    NOTE(440, 250);
    NOTE(440, 500);
    NOTE(493, 2000);
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 500);
    NOTE(520, 1000);
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 500);
    NOTE(392, 750);
    NOTE(440, 250);
    NOTE(493, 2500);

    NOTE(586, 500);
    NOTE(586, 500);
    NOTE(586, 500);
    NOTE(656, 1000);
    NOTE(493, 500);
    NOTE(493, 500);
    NOTE(586, 500);
    NOTE(586, 750);
    NOTE(440, 250);
    NOTE(440, 500);
    NOTE(520, 2000);

    NOTE(520, 500);
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 1500);
    NOTE(392, 500);
    NOTE(392, 500);
    NOTE(440, 750);
    NOTE(493, 250);
    NOTE(493, 2000);

    NOTE(586, 500);
    NOTE(586, 500);
    NOTE(586, 500);
    NOTE(656, 1000);
    NOTE(493, 500);
    NOTE(493, 500);
    NOTE(586, 500);
    NOTE(586, 750);
    NOTE(440, 250);
    NOTE(440, 500);
    NOTE(520, 2000);
    NOTE(520, 500);
    NOTE(493, 500);
    NOTE(440, 500);
    NOTE(493, 1500);
    NOTE(392, 500);
    NOTE(392, 500);
    NOTE(440, 1250);
    NOTE(392, 250);
    NOTE(392, 2500);
}

const char* convert_to_morse(float ax, float az)
{
        if(button)
        {
            button = BUTTON_NOT_PRESSED;
            dotnote();
            return ".\r\n";
        }
        else if(abs(az) > 1.3)
        {
            spacenote();
            return " \r\n";
        }
        else if (abs(ax) > 0.8)
        {
            dashnote();
            return "-\r\n";
        }
        else
        {
            return '\0';
        }
}

Void uartTaskFxn(UArg arg0, UArg arg1) {

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
    char prev[64];
    char debug_msg[64];
    char input;

    while (1) {
        if(programState == SENDING)
        {

            sprintf(debug_msg,"%s\0", convert_to_morse(ax_ma, az_ma));
            if(debug_msg[0]!='\0')
            {
                UART_write(uart, debug_msg, strlen(debug_msg)+1);
                if(prev[0]==' ' && debug_msg[0]==' ')
                {

                    programState = RECEIVING;
                }
                sprintf(prev, "%s", debug_msg);
                Task_sleep(700000 / Clock_tickPeriod);

            }


        }
        if(programState == RECEIVING)
        {


            UART_read(uart, &input, 1);
            if(input==45||input==46)
            {
                buzzerFxn((char)input);

            }

        }

        Task_sleep(10000 / Clock_tickPeriod);
    }
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
   if (programState==SENDING)
   {
       button = BUTTON_PRESSED;
   }
   else
   {
       UART_readCancel(uart);
       programState = SENDING;
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
    float ax1 = 0;
    float ax2 = 0;
    float ax3 = 0;
    float az1 = 0;
    float az2 = 0;
    float az3 = 0;
    // Loop forever
    while (1) {

        // MPU ask data
        mpu9250_get_data(&i2cMPU, &ax, &ay, &az, &gx, &gy, &gz);
        ax3 = ax2;
        ax2 = ax1;
        ax1 = fabs(ay);
        az3 = az2;
        az2 = az1;
        az1 = fabs(az);
        ax_ma = (ax1+ax2+ax3)/3.0;
        az_ma = (az1+az2+az3)/3.0;

        Task_sleep(20000 / Clock_tickPeriod);
    }

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

    hBuzzer = PIN_open(&sBuzzer, cBuzzer);
      if (hBuzzer == NULL) {
        System_abort("Pin open failed!");
      }
    buzzerSolo();
    buttonHandle = PIN_open(&buttonState, buttonConfig);
       if(!buttonHandle) {
          System_abort("Error initializing button pins\n");
       }
    // Open MPU power pin
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
        System_abort("Pin open failed!");
    }

    // Buzzer
    Task_Params_init(&taskParams);
    taskParams.stackSize = STACKSIZE;
    taskParams.stack = &taskStack;
    taskParams.priority=2;
    task = Task_create((Task_FuncPtr)sensorFxn, &taskParams, NULL);
    if (task == NULL) {
        System_abort("Task create failed!");
    }

    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
          System_abort("Error registering button callback function");
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
