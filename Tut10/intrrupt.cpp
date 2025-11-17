/******************************************************************************
* AXI Timer Interrupt Example
* 
* This application uses an AXI timer/counter to generate periodic interrupts.
* An instance of interrupt controller is used to handle interrupt signal generation.
* 
* Copyright (c) 2009 Xilinx, Inc. All rights reserved.
******************************************************************************/

/***************************** Include Files *********************************/

#include <iostream>
#include "xparameters.h"
#include "xil_types.h"
#include "xtmrctr.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xscugic.h"
#include <stdio.h>

using namespace std;

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are only defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define TIMER_DEVICE_ID         XPAR_AXI_TIMER_0_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_PS7_SCUGIC_0_DEVICE_ID
#define TIMER_INTERRUPT_ID      XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR

/*
 * Timer counter configuration
 * The timer will count from 0 to this value and then generate an interrupt
 * Clock frequency is typically 100MHz, so 0x05F5E100 = 100,000,000 = 1 second
 */
#define TIMER_LOAD_VALUE        0x05F5E100  // 1 second at 100MHz clock

/*
 * Timer counter number (0 or 1)
 * Each AXI Timer has two counters
 */
#define TIMER_COUNTER_0         0

/************************** Variable Definitions *****************************/

/* Instance of the Interrupt Controller */
XScuGic InterruptController;

/* The configuration parameters of the controller */
static XScuGic_Config *GicConfig;

/* Timer Instance */
XTmrCtr TimerInstancePtr;

/* Counter for tracking interrupts */
volatile int InterruptCounter = 0;

/* Flag to track if timer has been started */
volatile int TimerStarted = 0;

/************************** Function Prototypes ******************************/

void Timer_InterruptHandler(void *CallBackRef, u8 TmrCtrNumber);
int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr);
int ScuGicInterrupt_Init(u16 DeviceId, XTmrCtr *TimerInstancePtr);

/******************************************************************************/
/**
*
* Timer Interrupt Handler
* This function is called when a timer interrupt occurs.
* It increments a counter and prints a message.
*
* @param    CallBackRef is a pointer to the callback reference
* @param    TmrCtrNumber is the number of the timer generating the interrupt
*
* @return   None.
*
* @note     None.
*
******************************************************************************/
void Timer_InterruptHandler(void *CallBackRef, u8 TmrCtrNumber)
{
    XTmrCtr *InstancePtr = (XTmrCtr *)CallBackRef;
    
    /* Check if the interrupt is from the correct timer counter */
    if (XTmrCtr_IsExpired(InstancePtr, TmrCtrNumber)) {
        
        /* Increment interrupt counter */
        InterruptCounter++;
        
        /* Print interrupt message */
        printf("Timer interrupt occurred! Count: %d\r\n", InterruptCounter);
        
        /* Clear the interrupt flag - IMPORTANT! */
        /* This is done automatically by the driver for generate mode */
        
        /* Optional: Toggle an LED or perform other periodic tasks here */
        
        /* Optional: Stop after certain number of interrupts */
        if (InterruptCounter >= 10) {
            printf("Stopping timer after 10 interrupts\r\n");
            XTmrCtr_Stop(InstancePtr, TmrCtrNumber);
            TimerStarted = 0;
        }
    }
}

/******************************************************************************/
/**
*
* This function sets up the interrupt system for the timer.
*
* @param    XScuGicInstancePtr is a pointer to the instance of the ScuGic driver
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE
*
* @note     None.
*
******************************************************************************/
int SetUpInterruptSystem(XScuGic *XScuGicInstancePtr)
{
    /*
     * Connect the interrupt controller interrupt handler to the hardware
     * interrupt handling logic in the ARM processor.
     */
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler) XScuGic_InterruptHandler,
                                 XScuGicInstancePtr);
    
    /*
     * Enable interrupts in the ARM processor
     */
    Xil_ExceptionEnable();
    
    return XST_SUCCESS;
}

/******************************************************************************/
/**
*
* This function initializes the interrupt controller and connects the timer
* interrupt handler.
*
* @param    DeviceId is the Device ID of the interrupt controller
* @param    TimerInstancePtr is a pointer to the timer instance
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE
*
* @note     None.
*
******************************************************************************/
int ScuGicInterrupt_Init(u16 DeviceId, XTmrCtr *TimerInstancePtr)
{
    int Status;
    
    /*
     * Initialize the interrupt controller driver so that it is ready to use.
     */
    GicConfig = XScuGic_LookupConfig(DeviceId);
    if (NULL == GicConfig) {
        return XST_FAILURE;
    }
    
    Status = XScuGic_CfgInitialize(&InterruptController, GicConfig,
                                   GicConfig->CpuBaseAddress);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    
    /*
     * Setup the Interrupt System
     */
    Status = SetUpInterruptSystem(&InterruptController);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    
    /*
     * Connect a device driver handler that will be called when an
     * interrupt for the device occurs, the device driver handler performs
     * the specific interrupt processing for the device
     */
    Status = XScuGic_Connect(&InterruptController,
                            TIMER_INTERRUPT_ID,
                            (Xil_ExceptionHandler)XTmrCtr_InterruptHandler,
                            (void *)TimerInstancePtr);
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }
    
    /*
     * Enable the interrupt for the timer device
     */
    XScuGic_Enable(&InterruptController, TIMER_INTERRUPT_ID);
    
    return XST_SUCCESS;
}

/******************************************************************************/
/**
*
* Main function to demonstrate the use of the AXI Timer with interrupts.
*
* @param    None.
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE
*
* @note     None.
*
******************************************************************************/
int main()
{
    cout << "Application starts " << endl;
    int xStatus;
    
    // timer counter initialization
    xStatus = XTmrCtr_Initialize(&TimerInstancePtr, XPAR_AXI_TIMER_0_DEVICE_ID);
    if(XST_SUCCESS != xStatus)
    {
        cout << "timer counter initialization failed" ;
    }
    
    // timer handler
    XTmrCtr_SetHandler(&TimerInstancePtr, (XTmrCtr_Handler)Timer_InterruptHandler, &TimerInstancePtr);
    
    // initialize time pointer with value from xparameters.h file
    unsigned int* timer_ptr = (unsigned int*)XPAR_AXI_TIMER_0_BASEADDR;
    
    // load tlr
    *(timer_ptr + 1) = 0x00000000;
    
    // timer counter configuration
    // Configure timer in generate mode, count up, interrupt enabled
    // with autoreload of load register
    *(timer_ptr) = 0x0f4 ;
    
    xStatus=
    ScuGicInterrupt_Init(XPAR_PS7_SCUGIC_0_DEVICE_ID, &TimerInstancePtr);
    if(XST_SUCCESS != xStatus)
    {
        cout << " :( SCUGIC INIT FAILED )" << endl;
        return 1;
    }
    
    *(timer_ptr) = 0x0d4 ;  // deassert the load 5 to allow the timer to start counting
    
    // let timer run forever generating periodic interrupts
    
    while(1)
    {
        // infinite loop
    }
    
    return 0;
}

/******************************************************************************/
/**
* Alternative method using direct register access (optional)
* This shows how to set up the timer using direct register writes
* instead of the driver functions
******************************************************************************/
void Timer_DirectRegisterSetup(void)
{
    unsigned int *timer_ptr;
    
    /* Initialize timer pointer with base address from xparameters.h */
    timer_ptr = (unsigned int *)XPAR_AXI_TIMER_0_BASEADDR;
    
    /* Load Timer Load Register (TLR) with the reset value */
    *(timer_ptr + 1) = TIMER_LOAD_VALUE;  /* TLR0 offset = 4 bytes */
    
    /* Configure Timer Control/Status Register (TCSR) */
    /* Bit 11: CASC - Cascade mode (0 = no cascade)
     * Bit 10: ENALL - Enable all timers (0 = use enable bit)
     * Bit 9: PWMA - PWM mode (0 = generate mode)
     * Bit 8: T0INT - Timer 0 interrupt flag (read/clear)
     * Bit 7: ENT - Enable timer (1 = enable)
     * Bit 6: ENIT - Enable interrupt (1 = enable)
     * Bit 5: LOAD - Load timer (1 = load from TLR)
     * Bit 4: ARHT - Auto reload (1 = auto reload)
     * Bit 3: CAPT - Capture mode (0 = generate mode)
     * Bit 2: GENT - Generate enable (0 = external generate)
     * Bit 1: UDT - Up/Down count (0 = up)
     * Bit 0: MDT - Mode (0 = generate, 1 = capture)
     */
    
    /* Configure timer in generate mode, count up, interrupt enabled */
    /* with autoreload of load register */
    *(timer_ptr) = 0x00D4;  /* TCSR0: Enable interrupt, auto-reload, load TLR */
    
    /* Deassert the load bit to allow the timer to start counting */
    *(timer_ptr) = 0x00C4;  /* TCSR0: Clear load bit, keep other settings */
}
