/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include <string.h>


// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

uint8_t CACHE_ALIGN switchPromptUSB[] = "\r\nPUSH BUTTON PRESSED";
uint8_t CACHE_ALIGN miString[] = "                                 ";

uint8_t CACHE_ALIGN cdcReadBuffer[APP_READ_BUFFER_SIZE];
uint8_t CACHE_ALIGN cdcWriteBuffer[APP_READ_BUFFER_SIZE];


// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************



/*******************************************************
 * USB CDC Device Events - Application Event Handler
 *******************************************************/

USB_DEVICE_CDC_EVENT_RESPONSE APP_USBDeviceCDCEventHandler
(
    USB_DEVICE_CDC_INDEX index,
    USB_DEVICE_CDC_EVENT event,
    void * pData,
    uintptr_t userData
)
{
    APP_DATA * appDataObject;
    USB_CDC_CONTROL_LINE_STATE * controlLineStateData;
    USB_DEVICE_CDC_EVENT_DATA_READ_COMPLETE * eventDataRead;
    
    appDataObject = (APP_DATA *)userData;

    switch(event)
    {
        case USB_DEVICE_CDC_EVENT_GET_LINE_CODING:

            /* This means the host wants to know the current line
             * coding. This is a control transfer request. Use the
             * USB_DEVICE_ControlSend() function to send the data to
             * host.  */

            USB_DEVICE_ControlSend(appDataObject->deviceHandle,
                    &appDataObject->getLineCodingData, sizeof(USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_LINE_CODING:

            /* This means the host wants to set the line coding.
             * This is a control transfer request. Use the
             * USB_DEVICE_ControlReceive() function to receive the
             * data from the host */

            USB_DEVICE_ControlReceive(appDataObject->deviceHandle,
                    &appDataObject->setLineCodingData, sizeof(USB_CDC_LINE_CODING));

            break;

        case USB_DEVICE_CDC_EVENT_SET_CONTROL_LINE_STATE:

            /* This means the host is setting the control line state.
             * Read the control line state. We will accept this request
             * for now. */

            controlLineStateData = (USB_CDC_CONTROL_LINE_STATE *)pData;
            appDataObject->controlLineStateData.dtr = controlLineStateData->dtr;
            appDataObject->controlLineStateData.carrier = controlLineStateData->carrier;

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            break;

        case USB_DEVICE_CDC_EVENT_SEND_BREAK:

            /* This means that the host is requesting that a break of the
             * specified duration be sent. Read the break duration */

            appDataObject->breakData = ((USB_DEVICE_CDC_EVENT_DATA_SEND_BREAK *)pData)->breakDuration;
            
            /* Complete the control transfer by sending a ZLP  */
            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            
            break;

        case USB_DEVICE_CDC_EVENT_READ_COMPLETE:

            /* This means that the host has sent some data*/
            eventDataRead = (USB_DEVICE_CDC_EVENT_DATA_READ_COMPLETE *)pData;
            
            if(eventDataRead->status != USB_DEVICE_CDC_RESULT_ERROR)
            {
                appDataObject->isReadComplete = true;
                
                appDataObject->numBytesRead = eventDataRead->length; 
            }
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_RECEIVED:

            /* The data stage of the last control transfer is
             * complete. For now we accept all the data */

            USB_DEVICE_ControlStatus(appDataObject->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_SENT:

            /* This means the GET LINE CODING function data is valid. We don't
             * do much with this data in this demo. */
            break;

        case USB_DEVICE_CDC_EVENT_WRITE_COMPLETE:

            /* This means that the data write got completed. We can schedule
             * the next read. */

            appDataObject->isWriteComplete = true;
            break;

        default:
            break;
    }

    return USB_DEVICE_CDC_EVENT_RESPONSE_NONE;
}

/***********************************************
 * Application USB Device Layer Event Handler.
 ***********************************************/
void APP_USBDeviceEventHandler 
(
    USB_DEVICE_EVENT event, 
    void * eventData, 
    uintptr_t context 
)
{
    USB_DEVICE_EVENT_DATA_CONFIGURED *configuredEventData;

    switch(event)
    {
        case USB_DEVICE_EVENT_SOF:

            /* This event is used for switch debounce. This flag is reset
             * by the switch process routine. */
            appData.sofEventHasOccurred = true;
            
            break;

        case USB_DEVICE_EVENT_RESET:

            /* Update LED to show reset state */
            LED_Off();

            appData.isConfigured = false;

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Check the configuration. We only support configuration 1 */
            configuredEventData = (USB_DEVICE_EVENT_DATA_CONFIGURED*)eventData;
            
            if ( configuredEventData->configurationValue == 1)
            {
                /* Update LED to show configured state */
                LED_On();
                
                /* Register the CDC Device application event handler here.
                 * Note how the appData object pointer is passed as the
                 * user data */

                USB_DEVICE_CDC_EventHandlerSet(USB_DEVICE_CDC_INDEX_0, APP_USBDeviceCDCEventHandler, (uintptr_t)&appData);

                /* Mark that the device is now configured */
                appData.isConfigured = true;
            }
            
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(appData.deviceHandle);
            
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:
            
            /* VBUS is not available. We can detach the device */
            USB_DEVICE_Detach(appData.deviceHandle);
            
            appData.isConfigured = false;
            
            LED_Off();
            
            break;

        case USB_DEVICE_EVENT_SUSPENDED:

            /* Switch LED to show suspended state */
            LED_Off();
            
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            
            break;
    }
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

void APP_ProcessSwitchPress(void)
{
    /* This function checks if the switch is pressed and then
     * debounces the switch press*/
    
    if(SWITCH_STATE_PRESSED == (SWITCH_Get()))
    {
        if(appData.ignoreSwitchPress)
        {
            /* This means the key press is in progress */
            if(appData.sofEventHasOccurred)
            {
                /* A timer event has occurred. Update the debounce timer */
                appData.switchDebounceTimer ++;
                appData.sofEventHasOccurred = false;
                
                if (USB_DEVICE_ActiveSpeedGet(appData.deviceHandle) == USB_SPEED_FULL)
                {
                    appData.debounceCount = APP_USB_SWITCH_DEBOUNCE_COUNT_FS;
                }
                else if (USB_DEVICE_ActiveSpeedGet(appData.deviceHandle) == USB_SPEED_HIGH)
                {
                    appData.debounceCount = APP_USB_SWITCH_DEBOUNCE_COUNT_HS;
                }
                if(appData.switchDebounceTimer == appData.debounceCount)
                {
                    /* Indicate that we have valid switch press. The switch is
                     * pressed flag will be cleared by the application tasks
                     * routine. We should be ready for the next key press.*/
                    appData.isSwitchPressed = true;
                    appData.switchDebounceTimer = 0;
                    appData.ignoreSwitchPress = false;
                }
            }
        }
        else
        {
            /* We have a fresh key press */
            appData.ignoreSwitchPress = true;
            appData.switchDebounceTimer = 0;
        }
    }
    else
    {
        /* No key press. Reset all the indicators. */
        appData.ignoreSwitchPress = false;
        appData.switchDebounceTimer = 0;
        appData.sofEventHasOccurred = false;
    }
}

/*****************************************************
 * This function is called in every step of the
 * application state machine.
 *****************************************************/

bool APP_StateReset(void)
{
    /* This function returns true if the device
     * was reset  */

    bool retVal;

    if(appData.isConfigured == false)
    {
        appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
        appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
        appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
        appData.isReadComplete = true;
        appData.isWriteComplete = true;
        retVal = true;
    }
    else
    {
        retVal = false;
    }

    return(retVal);
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize(void)

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void)
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    
    /* Device Layer Handle  */
    appData.deviceHandle = USB_DEVICE_HANDLE_INVALID ;

    /* Device configured status */
    appData.isConfigured = false;

    /* Initial get line coding state */
    appData.getLineCodingData.dwDTERate = 9600;
    appData.getLineCodingData.bParityType = 0;
    appData.getLineCodingData.bParityType = 0;
    appData.getLineCodingData.bDataBits = 8;

    /* Read Transfer Handle */
    appData.readTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

    /* Write Transfer Handle */
    appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

    /* Initialize the read complete flag */
    appData.isReadComplete = true;

    /*Initialize the write complete flag*/
    appData.isWriteComplete = true;

    /* Initialize Ignore switch flag */
    appData.ignoreSwitchPress = false;

    /* Reset the switch debounce counter */
    appData.switchDebounceTimer = 0;

    /* Reset other flags */
    appData.sofEventHasOccurred = false;
    
    /* To know status of Switch */
    appData.isSwitchPressed = false;

    /* Set up the read buffer */
    appData.cdcReadBuffer = &cdcReadBuffer[0];

    /* Set up the read buffer */
    appData.cdcWriteBuffer = &cdcWriteBuffer[0];       
}


/******************************************************************************
  Function:
    void APP_Tasks(void)

  Remarks:
    See prototype in app.h.
 */


#define TRANS_COUNT 8
#define EDO_COUNT 9

char chr=0;
int acum1=0;
int acum2=0;
int res=0;
enum Oper{Suma,Resta,Mult,Div};
enum Oper oper;
int edo=0;
int edoAnt=0;
int trans=0;
int startIndex = 0, endIndex = 0; 
char numeroLeido[10];
char numeroAEscribir[11];
int miStringCont = 0;
int miPrintf_flag = 0;

bool estoyListo = false;
int cuantosDigitosVan = 0;
char auxString[] = "                                 ";


int chrTrans[TRANS_COUNT]=
					{ 0,'(',')','=',  8, 27, 6 , 7};
int mtzTrans[EDO_COUNT][TRANS_COUNT]={
					{ 0, 1 , 0 , 0 , 0 , 0 , 0 , 0},
					{ 1, 1 , 1 , 1 , 99, 99, 2 , 1},
					{ 2, 2 , 2 , 2 , 99, 99, 3 , 4},
					{ 3, 2 , 2 , 2 , 99, 99, 2 , 2},
					{ 4, 4 , 4 , 4 , 99, 99, 5 , 4},
					{ 5, 5 , 7 , 5 , 99, 99, 6 , 5},
					{ 6, 5 , 5 , 5 , 99, 99, 5 , 5},
					{ 7, 7 , 7 , 8 , 99, 99, 7 , 7},
					{ 8, 0 , 0 , 0 , 0 , 0 , 0 , 0}};

void miPrintf(char* s, int cont) {
    int i;
    for (i=0;i<cont;i++)
        miString[i]=s[i];
    miPrintf_flag=1;
    miStringCont=cont;
}
                

int calcTrans(char ch) {
	int tran=0;
	if ((ch>='0')&&(ch<='9'))	//Digito
		return(6);
	switch (ch) {
		case'+':
		case'-':
		case'*':
		case'/':
				return(7);
	}
	for (tran=5;tran>0;tran--)
		if (ch==chrTrans[tran])
			break;
	return(tran);
}

int sigEdo(int ed, int tran) {
	return(mtzTrans[ed][tran]);
}

int ejecutaEdo(int ed) {
    static int i=0;
    static int negativoFlag=0;
    static int digitosCont=0;
    static int auxRes=0;
	switch(ed) {
		case 0:
				break;
		case 1:
                //BSP_LEDOff( APP_USB_LED_1);
                //BSP_LEDOff( APP_USB_LED_2);
                //BSP_LEDOff( APP_USB_LED_3);
                LED_Off();
                LED2_Off();
                LED3_Off();

                acum1=0;
				miPrintf(&chr,1);
				break;
		case 2:
				miPrintf(&chr,1);
				acum1*=10;
				acum1+=(chr-'0');
				break;
		case 3:
				miPrintf(&chr,1);
				acum1*=10;
				acum1+=(chr-'0');
				return(2);
		case 4:
                LED_On();

                //BSP_LEDOn(  APP_USB_LED_1);
                LED2_Off();
                //BSP_LEDOff( APP_USB_LED_2);
                LED3_Off();
                //BSP_LEDOff( APP_USB_LED_3);
				miPrintf(&chr,1);
				switch (chr) {
					case'+':
							oper=Suma;
							break;
					case'-':
							oper=Resta;
							break;
					case'*':
							oper=Mult;
							break;
					case'/':
							oper=Div;
							break;
				}
				acum2=0;	//Preparar la entrada al estado 4
				break;
		case 5:
                LED_Off();
                LED2_On();
                LED3_Off();
                //BSP_LEDOff( APP_USB_LED_1);
                //BSP_LEDOn(  APP_USB_LED_2);
                //BSP_LEDOff( APP_USB_LED_3);
				miPrintf(&chr,1);
				acum2*=10;
				acum2=(chr-'0');
				break;
		case 6:
				miPrintf(&chr,1);
				acum2*=10;
				acum2+=(chr-'0');
				return(5);
		case 7:
                LED_Off();
                LED2_Off();
                LED3_On();
                //BSP_LEDOff( APP_USB_LED_1);
                //BSP_LEDOff( APP_USB_LED_2);
                //BSP_LEDOn(  APP_USB_LED_3);
				miPrintf(&chr,1);
				break;
		case 8:
                LED_On();
                LED2_On();
                LED3_On();
                //BSP_LEDOn( APP_USB_LED_1);
                //BSP_LEDOn( APP_USB_LED_2);
                //BSP_LEDOn( APP_USB_LED_3);
				switch(oper) {
					case Suma:
							res=acum1+acum2;
							break;
					case Resta:
							res=acum1-acum2;
							break;
					case Mult:
							res=acum1*acum2;
							break;
					case Div:
							if (acum2)
								res=acum1/acum2;
							else
								res=-1;
							break;
				}
				//printf("%d\n",res);
                if (res<0) {
                    negativoFlag=1;
                    res=-1*res;
                } else {
                    negativoFlag=0;
                }
                auxRes=res;
                digitosCont=0;
                do {
                    auxRes/=10;
                    digitosCont++;
                } while(auxRes);
                i=digitosCont;
                do {
                    auxString[negativoFlag+i]='0'+(res%10);
                    res/=10;
                } while(--i>=0);
                auxString[0]='=';
                if (negativoFlag) {
                    auxString[1]='-';
                }
                auxString[digitosCont+1+negativoFlag]=0x0D; //Carriage return
                miPrintf(&auxString[0],digitosCont+1+negativoFlag+1);
				return(0);
		case 99:
				//printf("\n<<<Captura cancelada>>>\n");
				return(0);	//Estado aceptor, rompe la rutina y marca estado de salida
	}
	return(edo);	//Para estados no aceptores regresar el estado ejecutado
}


void APP_Tasks(void)
{
    /* Update the application state machine based
     * on the current state */
    int i;
    
    switch(appData.state)
    {
        case APP_STATE_INIT:

            /* Open the device layer */
            appData.deviceHandle = USB_DEVICE_Open( USB_DEVICE_INDEX_0, DRV_IO_INTENT_READWRITE );

            if(appData.deviceHandle != USB_DEVICE_HANDLE_INVALID)
            {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.deviceHandle, APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            }
            else
            {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }

            break;

        case APP_STATE_WAIT_FOR_CONFIGURATION:

            /* Check if the device was configured */
            if(appData.isConfigured)
            {
                /* If the device is configured then lets start reading */
                appData.state = APP_STATE_SCHEDULE_READ;
            }
            
            break;

        case APP_STATE_SCHEDULE_READ:

            if(APP_StateReset())
            {
                break;
            }

            /* If a read is complete, then schedule a read
             * else wait for the current read to complete */

            appData.state = APP_STATE_WAIT_FOR_READ_COMPLETE;
            if(appData.isReadComplete == true)
            {
                appData.isReadComplete = false;
                appData.readTransferHandle =  USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;

                USB_DEVICE_CDC_Read (USB_DEVICE_CDC_INDEX_0,
                        &appData.readTransferHandle, appData.cdcReadBuffer,
                        APP_READ_BUFFER_SIZE);
                
                if(appData.readTransferHandle == USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID)
                {
                    appData.state = APP_STATE_ERROR;
                    break;
                }
            }

            break;

        case APP_STATE_WAIT_FOR_READ_COMPLETE:
        case APP_STATE_CHECK_SWITCH_PRESSED:

            if(APP_StateReset())
            {
                break;
            }

            APP_ProcessSwitchPress();

            /* Check if a character was received or a switch was pressed.
             * The isReadComplete flag gets updated in the CDC event handler. */

            if(appData.isReadComplete || appData.isSwitchPressed)
            {
                appData.state = APP_STATE_SCHEDULE_WRITE;
            }

            break;


        case APP_STATE_SCHEDULE_WRITE:

            if(APP_StateReset())
            {
                break;
            }

            /* Setup the write */

            appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
            appData.isWriteComplete = false;
            appData.state = APP_STATE_WAIT_FOR_WRITE_COMPLETE;

            if(appData.isSwitchPressed)
            {
                /* If the switch was pressed, then send the switch prompt*/
                appData.isSwitchPressed = false;
                USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0,
                        &appData.writeTransferHandle, switchPromptUSB, sizeof(switchPromptUSB),
                        USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
            }
            else
            {
                /* Else echo each received character by adding 1 */
                for(i = 0; i < appData.numBytesRead; i++)
                {   
                   
                    
                    if((appData.cdcReadBuffer[i] != 0x0A) && (appData.cdcReadBuffer[i] != 0x0D) /*&& (appData.cdcReadBuffer[i] >= '0' && appData.cdcReadBuffer[i] <= '9')*/ )
                    {
                        
                        //appData.readBuffer[i] = appData.readBuffer[i] + 1;
                        chr=appData.cdcReadBuffer[i];
                        trans=calcTrans(chr);	//Calcular la transici??n seg??n la entrada del teclado
                        if (trans) {			//Validar por transici??n valida (la transici??n 0 es inv??lida)
                            edoAnt=edo;					//Guardar el estado anterior
                            edo=sigEdo(edoAnt,trans);	//Calcular el siguiente estado
                            if (edoAnt!=edo)			//Solo si hay cambio de estado hay que ...
                                edo=ejecutaEdo(edo);	// ... ejecutar el nuevo estado y asignar estado de continuidad
                        }
                        
              
                  
                    } 
                    
                }
                if (miPrintf_flag) {
                    USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0, &appData.writeTransferHandle,
                                            miString, miStringCont, USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
                    miPrintf_flag=0;
                    miStringCont=0;
                }
              
            }

            break;

        case APP_STATE_WAIT_FOR_WRITE_COMPLETE:

            if(APP_StateReset())
            {
                break;
            }

            /* Check if a character was sent. The isWriteComplete
             * flag gets updated in the CDC event handler */

            if(appData.isWriteComplete == true)
            {
                appData.state = APP_STATE_SCHEDULE_READ;
            }

            break;

        case APP_STATE_ERROR:
        default:
            
            break;
    }
}

/*******************************************************************************
 End of File
 */

