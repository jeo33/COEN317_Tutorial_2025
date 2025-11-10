#include "stdbool.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xgpio.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xtmrctr.h"
#include <iostream>

using namespace std;

// define Timer0 0

int main()
{
    //variables
    XTmrCtr TimerInstancePtr;
    int xStatus;
    //AXI Timer Initialization and Setting
    xStatus = XTmrCtr_Initialize(&TimerInstancePtr, XPAR_AXI_TIMER_0_DEVICE_ID);
    if(xStatus != XST_SUCCESS)
    {
        cout << "TIMER INIT FAILED" << endl;
        return 0;
    }
    // reset value
    //count up configuration
    XTmrCtr_SetResetValue(&TimerInstancePtr, Timer0, 0xFFD23941);
    
    u32  TmrCtr_Ptr = (u32*) XPAR_TMRCTR_0_BASEADDR ; //defined in xparameter header file
    
    int offset = 0 ; //offset is set to 0 to get access to the TCSR0
    //write to the TCSR0,
    //activate GENT0, and load bit
    *(TmrCtr_Ptr +offset) = 0x0024 ;
    
    //start the timer
    XTmrCtr_Start(&TimerInstancePtr, TIMER0);
}
