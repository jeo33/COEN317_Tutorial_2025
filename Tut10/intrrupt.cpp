/******************************************************************************
* Copyright (C) 2005 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/***************************** Include Files *********************************/

#include "xcan.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xil_exception.h"

#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#include <stdio.h>
#else  /* SCU GIC */
#include "xscugic.h"
#include "xil_printf.h"
#endif

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define CAN_DEVICE_ID		XPAR_CAN_0_DEVICE_ID
#define CAN_INTR_VEC_ID		XPAR_INTC_0_CAN_0_VEC_ID

#ifdef XPAR_INTC_0_DEVICE_ID
 #define INTC_DEVICE_ID		XPAR_INTC_0_DEVICE_ID
#else
 #define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#endif /* XPAR_INTC_0_DEVICE_ID */

/* Maximum CAN frame size in words */
#define XCAN_MAX_FRAME_SIZE_IN_WORDS (XCAN_MAX_FRAME_SIZE / sizeof(u32))
#define FRAME_DATA_LENGTH	8  /* Frame Data field length */

/* Message ID for test */
#define TEST_MESSAGE_ID		1024

/*
 * The Baud Rate Prescaler Register (BRPR) and Bit Timing Register (BTR)
 * are setup such that CAN baud rate equals 40Kbps, assuming that the
 * CAN clock is 24MHz. These values should be modified based on the desired
 * baud rate and CAN clock frequency.
 */
#define TEST_BRPR_BAUD_PRESCALAR	29

#define TEST_BTR_SYNCJUMPWIDTH		3
#define TEST_BTR_SECOND_TIMESEGMENT	2
#define TEST_BTR_FIRST_TIMESEGMENT	15

/**************************** Type Definitions *******************************/

#ifdef XPAR_INTC_0_DEVICE_ID
 #define INTC		XIntc
 #define INTC_HANDLER	XIntc_InterruptHandler
#else
 #define INTC		XScuGic
 #define INTC_HANDLER	XScuGic_InterruptHandler
#endif /* XPAR_INTC_0_DEVICE_ID */

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

static int XCanIntrExample(u16 DeviceId);
static void Config(XCan *InstancePtr);
static void SendFrame(XCan *InstancePtr);

static void SendHandler(void *CallBackRef);
static void RecvHandler(void *CallBackRef);
static void ErrorHandler(void *CallBackRef, u32 ErrorMask);
static void EventHandler(void *CallBackRef, u32 Mask);

static int SetupInterruptSystem(XCan *InstancePtr);

/************************** Variable Definitions *****************************/

/* Driver instance */
static XCan Can;

/* Buffers for transmit and receive */
static u32 TxFrame[XCAN_MAX_FRAME_SIZE_IN_WORDS];
static u32 RxFrame[XCAN_MAX_FRAME_SIZE_IN_WORDS];

/* Flags for status */
volatile static int LoopbackError;	/* Asynchronous error occurred */
volatile static int RecvDone;		/* Received a frame */
volatile static int SendDone;		/* Frame was sent successfully */

/* For interrupt system */
static INTC InterruptController;

/******************************************************************************/
/**
*
* Main function to call the CAN Interrupt example.
*
* @param	None.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
int main(void)
{
	int Status;

	xil_printf("===== CAN Interface Example =====\r\n");
	xil_printf("Running CAN interrupt example...\r\n");

	/*
	 * Run the CAN interrupt example
	 */
	Status = XCanIntrExample(CAN_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("CAN Interrupt Example Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran CAN Interrupt Example\r\n");
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* The main entry point for showing the usage of XCan driver in interrupt
* mode. This function sends a CAN frame and receives the same CAN frame using
* the loopback mode.
*
* @param	DeviceId is the XPAR_CAN_<instance_num>_DEVICE_ID value from
*		xparameters.h.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		If the device is not working correctly, this function may enter
*		an infinite loop and will never return to the caller.
*
******************************************************************************/
static int XCanIntrExample(u16 DeviceId)
{
	int Status;

	/*
	 * Initialize the CAN driver
	 */
	Status = XCan_Initialize(&Can, DeviceId);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to initialize CAN driver\r\n");
		return XST_FAILURE;
	}

	/*
	 * Run self-test on the device
	 */
	Status = XCan_SelfTest(&Can);
	if (Status != XST_SUCCESS) {
		xil_printf("Self test failed\r\n");
		return XST_FAILURE;
	}

	/*
	 * Configure the CAN device
	 */
	Config(&Can);

	/*
	 * Set interrupt handlers
	 */
	XCan_SetHandler(&Can, XCAN_HANDLER_SEND,
			(void *)SendHandler, (void *)&Can);
	XCan_SetHandler(&Can, XCAN_HANDLER_RECV,
			(void *)RecvHandler, (void *)&Can);
	XCan_SetHandler(&Can, XCAN_HANDLER_ERROR,
			(void *)ErrorHandler, (void *)&Can);
	XCan_SetHandler(&Can, XCAN_HANDLER_EVENT,
			(void *)EventHandler, (void *)&Can);

	/*
	 * Initialize flags
	 */
	SendDone = FALSE;
	RecvDone = FALSE;
	LoopbackError = FALSE;

	/*
	 * Connect to processor interrupt
	 */
	Status = SetupInterruptSystem(&Can);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to setup interrupt system\r\n");
		return XST_FAILURE;
	}

	/*
	 * Enable all interrupts in CAN device
	 */
	XCan_InterruptEnable(&Can, XCAN_IXR_ALL);

	/*
	 * Enter loopback mode
	 */
	XCan_EnterMode(&Can, XCAN_MODE_LOOPBACK);
	while(XCan_GetMode(&Can) != XCAN_MODE_LOOPBACK);

	/*
	 * Send a frame
	 */
	xil_printf("Sending CAN frame...\r\n");
	SendFrame(&Can);

	/*
	 * Wait for the frame to be transmitted and received
	 */
	while ((SendDone != TRUE) || (RecvDone != TRUE));

	/*
	 * Check for errors found in the callbacks
	 */
	if (LoopbackError == TRUE) {
		xil_printf("Loopback test error\r\n");
		return XST_LOOPBACK_ERROR;
	}

	xil_printf("CAN frame sent and received successfully\r\n");
	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function configures the CAN device for the baud rate and timing
* parameters.
*
* @param	InstancePtr is a pointer to the driver instance
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void Config(XCan *InstancePtr)
{
	/*
	 * Enter configuration mode
	 */
	XCan_EnterMode(InstancePtr, XCAN_MODE_CONFIG);
	while(XCan_GetMode(InstancePtr) != XCAN_MODE_CONFIG);

	/*
	 * Set the baud rate prescaler and bit timing values
	 */
	XCan_SetBaudRatePrescaler(InstancePtr, TEST_BRPR_BAUD_PRESCALAR);
	XCan_SetBitTiming(InstancePtr, TEST_BTR_SYNCJUMPWIDTH,
			TEST_BTR_SECOND_TIMESEGMENT,
			TEST_BTR_FIRST_TIMESEGMENT);
}

/*****************************************************************************/
/**
*
* Send a CAN frame.
*
* @param	InstancePtr is a pointer to the driver instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void SendFrame(XCan *InstancePtr)
{
	u8 *FramePtr;
	int Index;
	int Status;

	/*
	 * Create the message ID
	 */
	TxFrame[0] = XCan_CreateIdValue(TEST_MESSAGE_ID, 0, 0, 0, 0);
	TxFrame[1] = XCan_CreateDlcValue(FRAME_DATA_LENGTH);

	/*
	 * Fill in the data field with incremental values (0, 1, 2...)
	 */
	FramePtr = (u8 *)(&TxFrame[2]);
	for (Index = 0; Index < FRAME_DATA_LENGTH; Index++) {
		*FramePtr++ = (u8)Index;
	}

	/*
	 * Wait until TX FIFO is not full
	 */
	while (XCan_IsTxFifoFull(InstancePtr) == TRUE);

	/*
	 * Send the frame
	 */
	Status = XCan_Send(InstancePtr, TxFrame);
	if (Status != XST_SUCCESS) {
		/*
		 * The frame could not be sent successfully
		 */
		xil_printf("Failed to send frame\r\n");
		LoopbackError = TRUE;
		SendDone = TRUE;
		RecvDone = TRUE;
	}
}

/*****************************************************************************/
/**
*
* This function is the interrupt handler for the send interrupt.
* It is called when a frame is transmitted successfully.
*
* @param	CallBackRef is a pointer to the driver instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void SendHandler(void *CallBackRef)
{
	/*
	 * The frame was sent successfully. Notify the task context.
	 */
	SendDone = TRUE;
}

/*****************************************************************************/
/**
*
* This function is the interrupt handler for the receive interrupt.
* It processes the received frame and checks its validity.
*
* @param	CallBackRef is a pointer to the driver instance.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void RecvHandler(void *CallBackRef)
{
	XCan *CanPtr = (XCan *)CallBackRef;
	int Status;
	int Index;
	u8 *FramePtr;

	/*
	 * Receive the frame
	 */
	Status = XCan_Recv(CanPtr, RxFrame);
	if (Status != XST_SUCCESS) {
		LoopbackError = TRUE;
		RecvDone = TRUE;
		return;
	}

	/*
	 * Verify the frame received is expected
	 */

	/* Check message ID */
	if (RxFrame[0] != XCan_CreateIdValue(TEST_MESSAGE_ID, 0, 0, 0, 0)) {
		xil_printf("Received wrong message ID\r\n");
		LoopbackError = TRUE;
		RecvDone = TRUE;
		return;
	}

	/* Check data length code */
	if (RxFrame[1] != XCan_CreateDlcValue(FRAME_DATA_LENGTH)) {
		xil_printf("Received wrong DLC\r\n");
		LoopbackError = TRUE;
		RecvDone = TRUE;
		return;
	}

	/* Check data field */
	FramePtr = (u8 *)(&RxFrame[2]);
	for (Index = 0; Index < FRAME_DATA_LENGTH; Index++) {
		if (*FramePtr++ != (u8)Index) {
			xil_printf("Received wrong data byte at index %d\r\n", Index);
			LoopbackError = TRUE;
			break;
		}
	}

	RecvDone = TRUE;
}

/*****************************************************************************/
/**
*
* This function is the interrupt handler for the error interrupt.
*
* @param	CallBackRef is a pointer to the driver instance.
* @param	ErrorMask is a mask that indicates the cause of the error.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void ErrorHandler(void *CallBackRef, u32 ErrorMask)
{
	XCan *CanPtr = (XCan *)CallBackRef;
	u32 Status;

	if(ErrorMask & XCAN_ESR_ACKER_MASK) {
		xil_printf("Received ACK Error\r\n");
	}
	if(ErrorMask & XCAN_ESR_BERR_MASK) {
		xil_printf("Received Bit Error\r\n");
	}
	if(ErrorMask & XCAN_ESR_STER_MASK) {
		xil_printf("Received Stuff Error\r\n");
	}
	if(ErrorMask & XCAN_ESR_FMER_MASK) {
		xil_printf("Received Form Error\r\n");
	}
	if(ErrorMask & XCAN_ESR_CRCER_MASK) {
		xil_printf("Received CRC Error\r\n");
	}

	/*
	 * Set error flag
	 */
	LoopbackError = TRUE;
	RecvDone = TRUE;
	SendDone = TRUE;

	/*
	 * Clear the error status
	 */
	Status = XCan_GetBusErrorStatus(CanPtr);
	XCan_ClearBusErrorStatus(CanPtr, Status);
}

/*****************************************************************************/
/**
*
* This function is the interrupt handler for the event interrupt.
*
* @param	CallBackRef is a pointer to the driver instance.
* @param	Mask is a mask that indicates the cause of the event.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
static void EventHandler(void *CallBackRef, u32 Mask)
{
	/* Handle various events */
	if (Mask & XCAN_IXR_BSOFF_MASK) {
		xil_printf("Bus Off event\r\n");
		LoopbackError = TRUE;
		RecvDone = TRUE;
		SendDone = TRUE;
	}
	if (Mask & XCAN_IXR_RXOFLW_MASK) {
		xil_printf("RX FIFO Overflow event\r\n");
	}
	if (Mask & XCAN_IXR_WKUP_MASK) {
		xil_printf("Wake up event\r\n");
	}
	if (Mask & XCAN_IXR_SLP_MASK) {
		xil_printf("Sleep event\r\n");
	}
	if (Mask & XCAN_IXR_ARBLST_MASK) {
		xil_printf("Arbitration lost event\r\n");
	}
}

/*****************************************************************************/
/**
*
* This function sets up the interrupt system so interrupts can occur for the
* CAN. This function is application-specific since the actual system may or
* may not have an interrupt controller.
*
* @param	InstancePtr is a pointer to the XCan instance.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
******************************************************************************/
static int SetupInterruptSystem(XCan *InstancePtr)
{
	int Status;

#ifdef XPAR_INTC_0_DEVICE_ID
	/*
	 * Initialize the interrupt controller driver
	 */
	Status = XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the device driver handler
	 */
	Status = XIntc_Connect(&InterruptController,
				CAN_INTR_VEC_ID,
				(XInterruptHandler)XCan_IntrHandler,
				InstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller
	 */
	Status = XIntc_Start(&InterruptController, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the CAN device
	 */
	XIntc_Enable(&InterruptController, CAN_INTR_VEC_ID);

	/*
	 * Initialize the exception table
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			&InterruptController);

	/*
	 * Enable exceptions
	 */
	Xil_ExceptionEnable();

#else /* SCUGIC */

	XScuGic_Config *IntcConfig;

	/*
	 * Initialize the interrupt controller driver
	 */
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(&InterruptController, IntcConfig,
				IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set priority and trigger type
	 */
	XScuGic_SetPriorityTriggerType(&InterruptController, CAN_INTR_VEC_ID,
					0xA0, 0x3);

	/*
	 * Connect the interrupt handler
	 */
	Status = XScuGic_Connect(&InterruptController, CAN_INTR_VEC_ID,
				(Xil_InterruptHandler)XCan_IntrHandler,
				InstancePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the CAN device
	 */
	XScuGic_Enable(&InterruptController, CAN_INTR_VEC_ID);

	/*
	 * Initialize the exception table
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			&InterruptController);

	/*
	 * Enable exceptions
	 */
	Xil_ExceptionEnable();

#endif /* XPAR_INTC_0_DEVICE_ID */

	return XST_SUCCESS;
}
