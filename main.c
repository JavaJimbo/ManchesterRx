/*********************************************************************************************
 * MAIN file for LINX MANCHESTER RXTX
 * For PIC32MX795F512X boards
 * 
 * 11-18-2016: Got transmitting and receiving working beautifully 
 * with Manchester coding on Parallax Linx 413 Mhz boards.
 * 11-19-2016: 
 *********************************************************************************************/

#define true TRUE
#define false FALSE

// #define TRANSMITTER
#define START_TRANSMIT 1
#define RECEIVER

#define TRIG_OUT LATBbits.LATB15
#define TRIG_BIT BIT_15
#define NUM_DATA_BITS 16

#ifdef TRANSMITTER
#define TX_OUT LATDbits.LATD3 // PORTDbits.RD3
#define TX_PIN BIT_3
#endif

#ifdef RECEIVER
// #define RX_IN PORTBbits.RB0
#define RX_IN PORTReadBits(IOPORT_B, BIT_0)
#endif

#define LINXuart UART1

#include <plib.h>


#pragma config UPLLEN   = ON            // USB PLL Enabled
#pragma config FPLLMUL  = MUL_20        // PLL Multiplier
#pragma config UPLLIDIV = DIV_2         // USB PLL Input Divider
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider
#pragma config FPLLODIV = DIV_1         // PLL Output Divider
#pragma config FPBDIV   = DIV_1         // Peripheral Clock divisor
#pragma config FWDTEN   = OFF           // Watchdog Timer
#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF           // CLKO Enable
#pragma config POSCMOD  = HS            // Primary Oscillator
#pragma config IESO     = OFF           // Internal/External Switch-over
#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable (KLO was off)
#pragma config FNOSC    = PRIPLL        // Oscillator Selection
#pragma config CP       = OFF           // Code Protect
#pragma config BWP      = OFF           // Boot Flash Write Protect
#pragma config PWP      = OFF           // Program Flash Write Protect
#pragma config ICESEL   = ICS_PGx2      // ICE/ICD Comm Channel Select


/** V A R I A B L E S ********************************************************/
#define HOSTuart UART2
#define HOSTbits U2STAbits
#define HOST_VECTOR _UART_2_VECTOR 

#define MAXBUFFER 128
unsigned char HOSTTxBuffer[MAXBUFFER];
unsigned char HOSTRxBuffer[MAXBUFFER];
unsigned short HOSTTxLength;
unsigned short HOSTRxLength;
unsigned long dataInInteger;

#define MAXPOTS 4

unsigned char command = 0;
unsigned char arrPots[MAXPOTS];
unsigned char displayMode = TRUE;
unsigned char error = 0;

/** P R I V A T E  P R O T O T Y P E S ***************************************/
unsigned long getOutData(unsigned long dataOut);
unsigned char getDataByte(unsigned long dataInInteger);
static void InitializeSystem(void);
void ProcessIO(void);

void ConfigAd(void);

// #define FILENAME "CUES.txt"
static const char FILENAME[] = "CUES.txt";
#ifdef TRANSMITTER
unsigned char TXstate = 0;
unsigned long dataOutInt = 0;
#endif
#ifdef RECEIVER
unsigned char RXstate = 0, previousRXstate = 0;
unsigned char dataReady = FALSE;
#endif

unsigned char getDataByte(unsigned long dataInInteger) {
    unsigned short i;
    unsigned short intMask = 0b0010;
    unsigned char byteMask = 0x01;
    unsigned char dataByte;

    dataByte = 0x00;
    for (i = 0; i < NUM_DATA_BITS; i++) {
        if (intMask & dataInInteger) dataByte = dataByte | byteMask;
        intMask = intMask << 2;
        byteMask = byteMask << 1;
    }
    return (~dataByte);
}

unsigned short getDataInteger(unsigned long dataInInteger) {
    unsigned short i;
    unsigned long longMask = 0b0010;
    unsigned short shortMask = 0x01;
    unsigned short dataInt;

    dataInt = 0x0000;
    for (i = 0; i < NUM_DATA_BITS; i++) {
        if (longMask & dataInInteger) dataInt = dataInt | shortMask;
        longMask = longMask << 2;
        shortMask = shortMask << 1;
    }
    return (~dataInt);
}

#define XORmask 0xAAAAAAAA

unsigned long getOutData(unsigned long dataOut) {
    unsigned long dataInt, intMask, xorInt;
    unsigned long i, dataMask;

    dataInt = 0x0000;
    dataMask = 0x0001;
    intMask = 0x0003;
    for (i = 0; i < NUM_DATA_BITS; i++) {
        if (dataMask & dataOut) dataInt = dataInt | intMask;
        dataMask = dataMask << 1;
        intMask = intMask << 2;
    }
    xorInt = XORmask ^ dataInt;
    return (xorInt);
}

int main(void) {
    unsigned short RXdataIn;
    unsigned short dataOut = 0;
    unsigned short command = 0;
    unsigned short check = 0;
    short i;

    InitializeSystem();
    TRIG_OUT = 0;
#ifdef TRANSMITTER    
    TX_OUT = 0;
#endif    

    DelayMs(200);

#ifdef RECEIVER
    printf("\rTESTING RECEIVER");
#endif

#ifdef TRANSMITTER
    printf("\rTESTING TRANSMITTER");
    dataOut = 0;
#endif    

    dataOut = 0xFF00;
    while (1) {
#ifdef TRANSMITTER                
        if (!TXstate) {
            DelayMs(100);
            dataOut = command;
            dataOut = dataOut | (command << 8);
            dataOutInt = getOutData(dataOut);
            command++;
            if (command > 0xFF) command = 0;
            TXstate = START_TRANSMIT;
        }
#endif        
#ifdef RECEIVER
        if (dataReady) {
            dataReady = FALSE;
            // RXdataIn = (short) getDataByte(dataInInteger);
            RXdataIn = getDataInteger(dataInInteger);
            command = RXdataIn & 0xFF;
            check = (RXdataIn & 0xFF00) >> 8;
            if (check == command) printf("\r%X", command);
            else printf("\rERROR: %x != %x", check, command);
        } else if (error) {
            printf("\rError: %d", error);
            error = 0;
        }
#endif        

    }//end while
}//end main

/********************************************************************
 * Function:        static void InitializeSystem(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        InitializeSystem is a centralize initialization
 *                  routine. All required USB initialization routines
 *                  are called from here.
 *
 *                  User application initialization routine should
 *                  also be called from here.                  
 *
 * Note:            None
 *******************************************************************/
static void InitializeSystem(void) {
    SYSTEMConfigPerformance(80000000); // was 60000000

    PORTSetPinsDigitalOut(IOPORT_B, TRIG_BIT);

    PORTSetPinsDigitalOut(IOPORT_E, BIT_0 | BIT_1 | BIT_2 | BIT_3);

    // Turn off JTAG so we get the pins back
    mJTAGPortEnable(false);

#ifdef TRANSMITTER    
    PORTSetPinsDigitalOut(IOPORT_D, TX_PIN);
    TX_OUT = 1;
#endif    

#ifdef RECEIVER
    PORTSetPinsDigitalIn(IOPORT_B, BIT_0);
#endif    

    PORTSetPinsDigitalOut(IOPORT_B, BIT_15);


    PORTSetPinsDigitalOut(IOPORT_C, BIT_3 | BIT_13 | BIT_14 | BIT_4);

    // Set up Port G outputs:
    PORTSetPinsDigitalOut(IOPORT_G, BIT_0);

    // Set up Timer 2
    T2CON = 0x00;
    T2CONbits.TCKPS2 = 0; // 1:8 Prescaler
    T2CONbits.TCKPS1 = 1;
    T2CONbits.TCKPS0 = 1;
    T2CONbits.T32 = 0; // TMRx and TMRy form separate 16-bit timers
    PR2 = 100;
    T2CONbits.TON = 1; // Let her rip
    // OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_256, 5208); For 60 frames/sec 

    // set up the core timer interrupt with a priority of 5 and zero sub-priority
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_5);

    // Set up main UART
    UARTConfigure(HOSTuart, UART_ENABLE_HIGH_SPEED | UART_ENABLE_PINS_TX_RX_ONLY);
    UARTSetFifoMode(HOSTuart, UART_INTERRUPT_ON_RX_NOT_EMPTY); //  | UART_INTERRUPT_ON_TX_DONE  
    UARTSetLineControl(HOSTuart, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
#define HOSTuart UART2
#define SYS_FREQ 80000000
    UARTSetDataRate(HOSTuart, SYS_FREQ, 57600);
    UARTEnable(HOSTuart, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));

    // Configure UART #2 Interrupts
    INTEnable(INT_U2TX, INT_DISABLED);
    INTEnable(INT_SOURCE_UART_RX(HOSTuart), INT_ENABLED);
    INTSetVectorPriority(INT_VECTOR_UART(HOSTuart), INT_PRIORITY_LEVEL_2);
    // INTSetVectorSubPriority(INT_VECTOR_UART(HOSTuart), INT_SUB_PRIORITY_LEVEL_0);        

    /*
    // Set up LINX UART
    UARTConfigure(LINXuart, UART_ENABLE_PINS_TX_RX_ONLY);    
    UARTSetLineControl(LINXuart, UART_DATA_SIZE_8_BITS | UART_PARITY_NONE | UART_STOP_BITS_1);
    UARTSetDataRate(LINXuart, SYS_FREQ, 9600);
    // UARTEnable(LINXuart, UART_ENABLE_FLAGS(UART_PERIPHERAL | UART_RX | UART_TX));
    UARTEnable(LINXuart, UART_DISABLE);
     */

    // Turn on the interrupts
    INTEnableSystemMultiVectoredInt();

}//end UserInit


// HOST UART interrupt handler it is set at priority level 2

void __ISR(HOST_VECTOR, ipl2) IntHostUartHandler(void) {
    static unsigned short HOSTRxIndex = 0;
    static unsigned char TxIndex = 0;
    unsigned char ch;

    if (HOSTbits.OERR || HOSTbits.FERR) {
        if (UARTReceivedDataIsAvailable(HOSTuart))
            ch = UARTGetDataByte(HOSTuart);
        HOSTbits.OERR = 0;
        HOSTRxIndex = 0;
    } else if (INTGetFlag(INT_SOURCE_UART_RX(HOSTuart))) {
        INTClearFlag(INT_SOURCE_UART_RX(HOSTuart));
        if (UARTReceivedDataIsAvailable(HOSTuart)) {
            ch = UARTGetDataByte(HOSTuart);
            if (ch != 0 && ch != '\n') {
                if (HOSTRxIndex < MAXBUFFER - 2)
                    HOSTRxBuffer[HOSTRxIndex++] = ch;
                if (ch == '\r') {
                    HOSTRxBuffer[HOSTRxIndex] = '\0';
                    HOSTRxLength = HOSTRxIndex;
                    HOSTRxIndex = 0;
                }
            }
            // UARTtimeout=UART_TIMEOUT;
        }
    }
    if (INTGetFlag(INT_SOURCE_UART_TX(HOSTuart))) {
        INTClearFlag(INT_SOURCE_UART_TX(HOSTuart));
        if (HOSTTxLength) {
            if (TxIndex < MAXBUFFER) {
                ch = HOSTTxBuffer[TxIndex++];
                if (TxIndex <= HOSTTxLength) {
                    while (!UARTTransmitterIsReady(HOSTuart));
                    UARTSendDataByte(HOSTuart, ch);
                } else {
                    while (!UARTTransmitterIsReady(HOSTuart));
                    HOSTTxLength = false;
                    TxIndex = 0;
                }
            } else {
                TxIndex = 0;
                HOSTTxLength = false;
                INTEnable(INT_SOURCE_UART_TX(HOSTuart), INT_DISABLED);
            }
        } else INTEnable(INT_SOURCE_UART_TX(HOSTuart), INT_DISABLED);
    }
}

unsigned long getLongInteger(unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3) {

    union {
        unsigned char byte[4];
        unsigned long lngInteger;
    } convert;

    convert.byte[0] = b0;
    convert.byte[1] = b1;
    convert.byte[2] = b2;
    convert.byte[3] = b3;

    return (convert.lngInteger);
}

unsigned short getShort(unsigned char b0, unsigned char b1) {

    union {
        unsigned char byte[2];
        unsigned short integer;
    } convert;

    convert.byte[0] = b0;
    convert.byte[1] = b1;
    return (convert.integer);
}

void ConfigAd(void) {

    mPORTBSetPinsAnalogIn(BIT_0);
    mPORTBSetPinsAnalogIn(BIT_1);
    mPORTBSetPinsAnalogIn(BIT_2);
    mPORTBSetPinsAnalogIn(BIT_3);

    // ---- configure and enable the ADC ----

    // ensure the ADC is off before setting the configuration
    CloseADC10();

    // define setup parameters for OpenADC10
    //                 Turn module on | ouput in integer | trigger mode auto | enable autosample
#define PARAM1  ADC_MODULE_ON | ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON

    // ADC ref external    | disable offset test    | enable scan mode | perform  samples | use dual buffers | use only mux A
#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_ON | ADC_SAMPLES_PER_INT_4 | ADC_ALT_BUF_ON | ADC_ALT_INPUT_OFF

    //                   use ADC internal clock | set sample time
#define PARAM3  ADC_CONV_CLK_INTERNAL_RC | ADC_SAMPLE_TIME_31

    //  set inputs to analog
#define PARAM4    ENABLE_AN0_ANA | ENABLE_AN1_ANA| ENABLE_AN2_ANA | ENABLE_AN3_ANA

    // Only scan AN0, AN1, AN2, AN3 for now
#define PARAM5   SKIP_SCAN_AN4 |SKIP_SCAN_AN5 |SKIP_SCAN_AN6 |SKIP_SCAN_AN7 |\
                    SKIP_SCAN_AN8 |SKIP_SCAN_AN9 |SKIP_SCAN_AN10 |\
                      SKIP_SCAN_AN11 | SKIP_SCAN_AN12 |SKIP_SCAN_AN13 |SKIP_SCAN_AN14 |SKIP_SCAN_AN15

    // set negative reference to Vref for Mux A
    SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF);

    // open the ADC
    OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);

    ConfigIntADC10(ADC_INT_PRI_2 | ADC_INT_SUB_PRI_2 | ADC_INT_ON);

    // clear the interrupt flag
    mAD1ClearIntFlag();

    // Enable the ADC
    EnableADC10();

}

void __ISR(_ADC_VECTOR, ipl6) AdcHandler(void) {
    unsigned short offSet;
    unsigned char i;

    mAD1IntEnable(INT_DISABLED);
    mAD1ClearIntFlag();

    // Determine which buffer is idle and create an offset
    offSet = 8 * ((~ReadActiveBufferADC10() & 0x01));

    for (i = 0; i < MAXPOTS; i++)
        arrPots[i] = (unsigned char) (ReadADC10(offSet + i) / 4); // read the result of channel 0 conversion from the idle buffer

}

#define ERROR_COUNTS 20
#define BIT_ERROR_COUNTS 6
#define START_ONE 200
#define START_TWO 60  // was 60
#define START_THREE 60 // was 60
#define BITPERIOD 10
#define TIMEOUT 30
#define NUM_SETTLING_PULSES 3 // was 8

void __ISR(_TIMER_2_VECTOR, ipl5) Timer2Handler(void) {
    static unsigned short Timer2Counter = 0;
    static unsigned long dataOutMask = 0x0001;
    static unsigned int PreviousPINstate = 0;
    static unsigned long dataInt = 0x0000;
    static unsigned char pulseCounter = 0;
#ifdef TRANSMITTER    
    static unsigned char bitCounter = 0;
#endif

    mT2ClearIntFlag(); // clear the interrupt flag    

#ifdef RECEIVER
    Timer2Counter++;
    if (RX_IN != PreviousPINstate) {
        PreviousPINstate = RX_IN;
        if (!RXstate) {
            if (RX_IN == 1) {
                RXstate++;
                Timer2Counter = 0x00;
                dataOutMask = 0x0001;
                dataInt = 0x0000;
            } else RXstate = 0;
        } else if (RXstate == 1) {
            if (RX_IN == 0) {
                if (Timer2Counter > (START_ONE - ERROR_COUNTS) && Timer2Counter < (START_ONE + ERROR_COUNTS)) {
                    RXstate++;
                    Timer2Counter = 0x00;
                } else RXstate = 0;
            } else RXstate = 0;
        } else if (RXstate == 2) {
            if (RX_IN == 1) {
                if (Timer2Counter > (START_TWO - ERROR_COUNTS) && Timer2Counter < (START_TWO + ERROR_COUNTS)) {
                    RXstate++;
                    Timer2Counter = 0x00;
                } else {
                    RXstate = 0;
                    // error = 1;
                }
            } else {
                RXstate = 0;
                error = 2;
            }
        } else if (RXstate == 3) {
            if (RX_IN == 0) {
                if (Timer2Counter > (START_THREE - ERROR_COUNTS) && Timer2Counter < (START_THREE + ERROR_COUNTS)) {
                    RXstate++;
                    Timer2Counter = 0x00;
                } else {
                    RXstate = 0;
                    error = 3;
                }
            } else {
                RXstate = 0;
                error = 4;
            }
        } else if (RXstate == 4) {
            if (Timer2Counter > (BITPERIOD - BIT_ERROR_COUNTS) && Timer2Counter < (BITPERIOD + BIT_ERROR_COUNTS)) {
                Timer2Counter = 0x00;
                if (RX_IN) RXstate++;
                else {
                    RXstate = 0;
                    error = 5;
                }
            } else {
                RXstate = 0;
                if (Timer2Counter <= (BITPERIOD - BIT_ERROR_COUNTS)) error = 6;
                else error = 7;
            }
        } else {
            if (Timer2Counter > (BITPERIOD - BIT_ERROR_COUNTS) && Timer2Counter < (BITPERIOD + BIT_ERROR_COUNTS)) {
                Timer2Counter = 0x00;
                if (RX_IN) dataInt = dataInt | dataOutMask;
                dataOutMask = dataOutMask << 1;
            } else if (Timer2Counter < ((BITPERIOD + BIT_ERROR_COUNTS)*2)) {
                Timer2Counter = 0x00;
                if (RX_IN) {
                    dataInt = dataInt | dataOutMask;
                    dataOutMask = dataOutMask << 1;
                    dataInt = dataInt | dataOutMask;
                    dataOutMask = dataOutMask << 1;
                } else dataOutMask = dataOutMask << 2;
            } else {
                RXstate = 0;
                error = 8;
            }
        }        
    } else if (Timer2Counter > TIMEOUT && RXstate == 5) {
        dataReady = TRUE;
        dataInInteger = dataInt;
        RXstate = 0;
    }     
#endif    

#ifdef TRANSMITTER     
    if (Timer2Counter) Timer2Counter--;
    if (!Timer2Counter) {
        if (TXstate) {
            if (TXstate == 1) {
                if (TX_OUT) {
                    TX_OUT = 0;
                    pulseCounter++;
                    if (pulseCounter >= NUM_SETTLING_PULSES) {
                        pulseCounter = 0;
                        TXstate++;
                    }
                } else TX_OUT = 1;
                TRIG_OUT = 1;
                Timer2Counter = BITPERIOD;
            } else if (TXstate == 2) {
                TX_OUT = 1;
                Timer2Counter = START_ONE;
                bitCounter = 0;
                dataOutMask = 0x0001;
                TXstate++;
            } else if (TXstate == 3) {
                TX_OUT = 0;
                Timer2Counter = START_TWO;
                TXstate++;
            } else if (TXstate == 4) {
                TX_OUT = 1;
                Timer2Counter = START_THREE;
                TXstate++;
            } else if (TXstate == 5) {
                TX_OUT = 0;
                Timer2Counter = BITPERIOD;
                TXstate++;
            } else if (TXstate == 6) {
                TX_OUT = 1;
                Timer2Counter = BITPERIOD;
                TXstate++;
            } else if (TXstate == 7) {
                if (bitCounter < (NUM_DATA_BITS * 2)) {
                    if (dataOutInt & dataOutMask) TX_OUT = 1;
                    else TX_OUT = 0;
                    dataOutMask = dataOutMask << 1;
                    Timer2Counter = BITPERIOD;
                } else {
                    TX_OUT = 0;
                    TXstate++;
                    Timer2Counter = TIMEOUT * 2;
                }
                bitCounter++;
            } else if (TXstate == 8) {
                TXstate = 0;
                TX_OUT = 0;
                TRIG_OUT = 0;
            } else {
                TXstate = 0;
                TX_OUT = 0;
                TRIG_OUT = 0;
            }
        }
    }
#endif       
}


/** EOF main.c *************************************************/

