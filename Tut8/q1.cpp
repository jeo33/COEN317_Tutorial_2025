#include "stdbool.h"
#include "xparameters.h"
#include "xil_types.h"
#include "xgpio.h"
#include "xil_io.h"
#include "xil_exception.h"
//#include <stdio.h>      // for C programs
#include <iostream>        // for C++ programs

#define AXI_GPIO_Example_ID XPAR_GPIO_0_DEVICE_ID

using namespace std;

int main()
{
    // step 2.1: variables
    static XGpio GPIOInstance_Ptr;
    int xStatus;
    
    //Step-2.2: AXI GPIO Initialization
    xStatus = XGpio_Initialize(&GPIOInstance_Ptr, AXI_GPIO_Example_ID);
    if(xStatus != XST_SUCCESS)
    {
        cout << "GPIO A Initialization FAILED" << endl;
        return 1;
    }
    
    //Step-2.3: AXI GPIO Set the Direction
    //channel 0 to connect to the LEDs
    XGpio_SetDataDirection(&GPIOInstance_Ptr, 1, 0x00);
    
    // channel 1 to be connected to the switches (3 bits)
    XGpio_SetDataDirection(&GPIOInstance_Ptr, 1, 0x07);
    
    while (1) {
        u32 read_switch = XGpio_DiscreteRead(GPIOInstance_Ptr, 1);
        
        switch(read_switch)
        {
            case 0:  // Binary 000 -> BCD 0000
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x00);
                break;
            case 1:  // Binary 001 -> BCD 0001
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x01);
                break;
            case 2:  // Binary 010 -> BCD 0010
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x02);
                break;
            case 3:  // Binary 011 -> BCD 0011
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x03);
                break;
            case 4:  // Binary 100 -> BCD 0100
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x04);
                break;
            case 5:  // Binary 101 -> BCD 0101
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x05);
                break;
            case 6:  // Binary 110 -> BCD 0110
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x06);
                break;
            case 7:  // Binary 111 -> BCD 0111
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x07);
                break;
            default:
                XGpio_DiscreteWrite(GPIOInstance_Ptr, 1, 0x00);
                break;
        }
    }
    
    return 0;
}
