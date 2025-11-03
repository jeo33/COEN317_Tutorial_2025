#include "stdbool.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xgpio.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xtmrctr.h"
#include <iostream>

#define CHANNEL0 0
#define CHANNEL1 1
#define AXI_GPIO_Example_ID XPAR_GPIO_0_DEVICE_ID

using namespace std;

int main()
{
    // step 2.1: variables
    XTmrCtr TimerInstancePtr;
    int xStatus1;
    
    static XGpio GPIOInstance_Ptr;
    int xStatus2;
    
    //Step-2.2: AXI GPIO Initialization
    xStatus1= XGpio_Initialize(&GPIOInstance_Ptr, AXI_GPIO_Example_ID);
    if(xStatus1 != XST_SUCCESS)
    {
        cout << "GPIO A Initialization FAILED" << endl;
        return 1;
    }
    
    // pin 0: input pin to be connected to the generate out signal
    XGpio_SetDataDirection(&Gpio, CHANNEL0, 0x01);
    
    // pin 0 to be connected to the LED
    XGpio_SetDataDirection(&Gpio, CHANNEL1, 0x00);
    XTmrCtr_SetResetValue(&TimerInstancePtr, 0, 0x61A8);
    
    // alternative to set the option
    //XTmrCtr_SetOptions(&TimerInstancePtr, XPAR_AXI_TIMER_0_DEVICE_ID, XTC_GENERATE_MODE_OPTION);
    
    u32 TmrCtr_Ptr = (u32*) XPAR_TMRCTR_0_BASEADDR; //defined in xparameter header file
    int offset = 0; //offset is set to 0 to get access to the TCSR0
    *(TmrCtr_Ptr + offset) = 0x000B6; //write to the TCSR0
    // (timer/counter control/status register of the timer 0)
    
    while (1) {
        XTmrCtr_Start(&TimerInstancePtr, 0); //start the timer
        
        //genout signal must be connected to the GPIO input pin
        //in the hardware development phase in Vivado
        u32 ReadGenOut = XGpio_DiscreteRead(&GPIOInstance_Ptr, CHANNEL1);
        
        if (ReadGenOut)
            XGpio_DiscreteWrite(&GPIOInstance_Ptr, CHANNEL2, 1);
        else
            XGpio_DiscreteWrite(&GPIOInstance_Ptr, CHANNEL2, 0);
    }
    
    return 0;
}
