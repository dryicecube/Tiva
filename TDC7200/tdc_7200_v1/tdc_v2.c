// Changes from v1*************************************************************
//! best performance observed was around 2K counts per second
//! possible causes:
//! 1. UART baud rate
//! 2. SPI read-write (after looking at the data, highly improbably to be the limiting factor.
//! 3. Maths on board.
//******************************************************************************
// In this version, the baud rate is changed to the maximum possible value.
// Changed clock source to system clock instead of PIOSC (16MHz)
//
//      UARTClockSourceSet(UART0_BASE, SysCtlClockGet());
//      UARTStdioConfig(0, 1000000 , SysCtlClockGet());
//
//
//
//

//*****************************************************************************

#include <stdint.h>
#include"inc/tm4c123gh6pm.h"
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "driverlib/interrupt.h"
#include "driverlib/ssi.h"

//*****************************************************************************
//! UART0, connected to the Virtual Serial Port and running at
//! 115,200, 8-N-1, is used to display messages from this application.
//*****************************************************************************

//*****************************************************************************
// The error routine that is called if the driver library encounters an error.
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif
#define TDC7200_CLOCK_FREQ                      (8000000u)// HZ
#define TDC7200_ENABLE_LOW_MS                   (5)
#define TDC7200_ENABLE_T3_LDO_SET3_MS           (2)
#define TDC7200_cal2_Period                     (10)
#define TDC7200_SPI_REG_ADDR_MASK               (0x1Fu)
#define TDC7200_SPI_REG_READ                    (0x00u)
#define TDC7200_SPI_REG_WRITE                   (0X40u)
#define TDC7200_SPI_REG_ADDR_MASK               (0x1Fu)


#define PS_PER_SEC                              (1000000000000)     // Physical Constants
#define US_PER_SEC                              (1000000)


#define TDC7200_REG_ADR_CONFIG1                (0x00u)   // Reg addresses
#define TDC7200_REG_ADR_CONFIG2                (0x01u)
#define TDC7200_REG_ADR_INT_STATUS             (0x02u)
#define TDC7200_REG_ADR_INT_MASK               (0x03u)
#define TDC7200_REG_ADR_TIME1                  (0x10u)
#define TDC7200_REG_ADR_CLOCK_COUNT1           (0x11u)
#define TDC7200_REG_ADR_TIME2                  (0x12u)
#define TDC7200_REG_ADR_CLOCK_COUNT2           (0x13u)
#define TDC7200_REG_ADR_CALIBRATION1           (0x1Bu)
#define TDC7200_REG_ADR_CALIBRATION2           (0x1Cu)

#define TDC7200_REG_DEFAULTS_CONFIG2           (0x40u)      // reset defaults
#define TDC7200_REG_DEFAULTS_INT_MASK          (0x07u)      // reset defaults



#define TDC7200_REG_SHIFT_INT_MASK_CLOCK_CNTR_OVF_MASK   (0x04u)       //Bit wise position
#define TDC7200_REG_SHIFT_INT_MASK_COARSE_CNTR_OVF_MASK  (0x02u)
#define TDC7200_REG_SHIFT_INT_MASK_NEW_MEAS_MASK         (0x01u)
#define TDC7200_REG_SHIFT_CONFIG1_MEAS_MODE_2            (0x02u)
#define TDC7200_REG_SHIFT_FORCE_CAL                      (0x80u)

#define TDC7200_REG_SHIFT_INT_STATUS_MEAS_COMPLETE_FLAG  (0x10u)
#define TDC7200_REG_SHIFT_INT_STATUS_MEAS_STARTED_FLAG   (0x08u)
#define TDC7200_REG_SHIFT_INT_STATUS_CLOCK_CNTR_OVF_INT  (0x04u)
#define TDC7200_REG_SHIFT_INT_STATUS_COARSE_CNTR_OVF_INT (0x02u)
#define TDC7200_REG_SHIFT_INT_STATUS_NEW_MEAS_INT        (0x01u)



char c='\n';
uint32_t junk;
//*****************************************************************************
// Function Definitions before they are moved into a header file
//*****************************************************************************
void ConfigureUART(void);                               //Configure UART for displaying
void GPIOEnable(void);                                  // Setup ENABLE pin
void GPIOINT(void);                                     // Setup INT pin with an ISR
void TDC7200_INT(void);                                 // ISR for INT pin
bool begin(void);                                       // Initial Config of TDC
void startMeasurement(void);                            // Start Measurement
void SPIenable(void);                                        // SPI0 Enable without CS pin.
uint32_t spiReadReg8(const uint8_t addr);               // SPI read 8
void spiWriteReg8(const uint8_t addr, const uint8_t val); //SPI write 8
uint32_t spiReadReg24(const uint8_t addr);              // SPI read 24

static void ui64toa(uint64_t v, char * buf, uint8_t base);

//*****************************************************************************
// Main
//*****************************************************************************
int main(void)
{
    //volatile uint32_t ui32Loop;

    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    FPUEnable();
    FPULazyStackingEnable();
    SysCtlClockSet(SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ |SYSCTL_OSC_MAIN|SYSCTL_SYSDIV_2_5);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
    ConfigureUART();
    GPIOEnable();
    GPIOINT();
    SPIenable();


    UARTprintf("Starting TDC7200!\n");
//    while(1)
//        {
//        begin();
//        //UARTprintf("Help!\n");
//        }

    while( !begin())
    {
        UARTprintf("Comms check failed\n");
        SysCtlDelay(SysCtlClockGet());

    }
    UARTprintf("Comms check passed\n");
// No changes need to be made in the Config 2 register. We are operating at the default operating points.
    //spiReadReg8(TDC7200_REG_ADR_CONFIG1);
    startMeasurement();
    //spiReadReg8(TDC7200_REG_ADR_CONFIG1);

    const uint8_t shift=20;
    UARTprintf("Measurement Started\n");
//    double test=0ull;
//    while (1){
//    test = (0.05);
//    UARTprintf("'%x'\n",test);
//    }
    return 0;
}




//*****************************************************************************
// Configure the UART and its pins.  This must be called before UARTprintf().
//*****************************************************************************
void
ConfigureUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    //UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTClockSourceSet(UART0_BASE, SysCtlClockGet());
    //UARTStdioConfig(0, 115200, 16000000);
    UARTStdioConfig(0, 1000000 , SysCtlClockGet());
    UARTFlowControlSet(UART0_BASE, UART_FLOWCONTROL_NONE) ; // Set UART flow control - NONE
    UARTFIFOEnable(UART0_BASE) ; // UART FIFO enable
    //IntEnable(INT_UART0); // Interrupt Enable
    //UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT); //pin interrupt enabled
}




//******************************************************************************
// Enables the relevant GPIO Pins
// Enables the ENB pin that resets the TDC7200
//******************************************************************************

void GPIOEnable(void)

{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB));
    GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_5);
}

//******************************************************************************
// Sends a positive edge on the ENABLE pin that resets the TDC7200 to default
// register values.
// Performs a comms sanity check
// Enables the interrupts for New Measurement result, Counter Overflow
//******************************************************************************


bool begin(void)
{

    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_5,0);            //Taking enable pin low
    SysCtlDelay(SysCtlClockGet() / (1000 * 3));             //waiting for some time
    GPIOPinWrite(GPIO_PORTB_BASE, GPIO_PIN_5,GPIO_PIN_5);   //Taking enable high to show a rising edge
    SysCtlDelay(SysCtlClockGet() / (1000 * 3));             //waiting for LDO to become stable.
    if ((spiReadReg8(TDC7200_REG_ADR_CONFIG2)!=TDC7200_REG_DEFAULTS_CONFIG2)|                     //Comms check if the SPI is working
            (spiReadReg8(TDC7200_REG_ADR_INT_MASK)!=TDC7200_REG_DEFAULTS_INT_MASK))
        {
           return false;
       }


    spiWriteReg8(TDC7200_REG_ADR_INT_MASK,TDC7200_REG_SHIFT_INT_MASK_NEW_MEAS_MASK             // Enabling interrupt for measurement result and overflow
                 |TDC7200_REG_SHIFT_INT_MASK_COARSE_CNTR_OVF_MASK
                 |TDC7200_REG_SHIFT_INT_MASK_CLOCK_CNTR_OVF_MASK);
    return true;
}
//******************************************************************************
// Enables the relevant SSI2
// Note: The chip select is a separate pin as the CS needs to be low for the
//       entire duration of transaction.
//******************************************************************************
void SPIenable(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);             // Clock enabled for SSI0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);            // Clock enabled for GPIO pin
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);            // Clock enabled for CS pin
    GPIOPinConfigure(GPIO_PB4_SSI2CLK);                     // Pin configuration set
    //GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    //GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_3);
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PB6_SSI2RX);
    GPIOPinConfigure(GPIO_PB7_SSI2TX);
    GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_7 | GPIO_PIN_6 | // SSI pin enabled
                   GPIO_PIN_4);
    SSIConfigSetExpClk(SSI2_BASE, 20000000, SSI_FRF_MOTO_MODE_0,         // SSI mode selected (20 MHz SCLK)
                       SSI_MODE_MASTER, 5000000, 8);
//    SSIConfigSetExpClk(SSI0_BASE, 20000000, SSI_FRF_TI,         // SSI mode selected
//                        SSI_MODE_MASTER, 1000000, 8);
    //SSIAdvModeSet(SSI0_BASE,)
    SSIEnable(SSI2_BASE);
    //GPIOPinWrite(GPIO_PORTA_BASE, GPIO_PIN_3, GPIO_PIN_3);// Enabled SSI
    while(SSIDataGetNonBlocking(SSI2_BASE, &junk))    // Cleaned any junk
    {
    }

}
uint32_t spiReadReg8(const uint8_t addr)
{
    uint32_t val;
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);// Enabled SSI
    SSIDataPut(SSI2_BASE, ((addr & TDC7200_SPI_REG_ADDR_MASK)|TDC7200_SPI_REG_READ));
    SSIDataGet(SSI2_BASE, &junk);
    junk&= 0x00FF;

    SSIDataPut(SSI2_BASE,0u);

    SSIDataGet(SSI2_BASE, &val);
    val&= 0x000000FF;

    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_PIN_0);// Enabled SSI

    return val;

}

void spiWriteReg8(const uint8_t addr, const uint8_t val)
{
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0,0); //CS Low
    SSIDataPut(SSI2_BASE, ((addr & TDC7200_SPI_REG_ADDR_MASK)|TDC7200_SPI_REG_WRITE));   //Write the address along with the masks
    SSIDataGet(SSI2_BASE, &junk);                                                       //Empty FIFO
    SSIDataPut(SSI2_BASE, val);                                                         // Put value
    SSIDataGet(SSI2_BASE, &junk);                                                       //Empty FIFO
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0,GPIO_PIN_0);                                // CS deassert
}

void startMeasurement(void)
{

    // Clear interrupt status
    spiWriteReg8(TDC7200_REG_ADR_INT_STATUS,TDC7200_REG_SHIFT_INT_STATUS_MEAS_COMPLETE_FLAG|
                 TDC7200_REG_SHIFT_INT_STATUS_MEAS_STARTED_FLAG
                 | TDC7200_REG_SHIFT_INT_STATUS_CLOCK_CNTR_OVF_INT
                 | TDC7200_REG_SHIFT_INT_STATUS_COARSE_CNTR_OVF_INT
                 | TDC7200_REG_SHIFT_INT_STATUS_NEW_MEAS_INT);

    // Select Mode 2 for operation for operations more than 500 ns
    spiWriteReg8(TDC7200_REG_ADR_CONFIG1,TDC7200_REG_SHIFT_CONFIG1_MEAS_MODE_2
                 |TDC7200_REG_SHIFT_FORCE_CAL|TDC7200_REG_SHIFT_INT_MASK_NEW_MEAS_MASK);  //Start the measurement
}



void GPIOINT()
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinTypeGPIOInput(GPIO_PORTA_BASE, GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTA_BASE, GPIO_PIN_4,GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);
    GPIOIntTypeSet(GPIO_PORTA_BASE,GPIO_PIN_4 ,GPIO_FALLING_EDGE);
    GPIOIntRegister(GPIO_PORTA_BASE, TDC7200_INT);
    GPIOIntEnable(GPIO_PORTA_BASE, GPIO_INT_PIN_4);
}

void TDC7200_INT ()
{
    const uint8_t shift = 20u;
    int64_t calCount =0ull;
    int64_t normLSB = 0ull;
    uint64_t tof = 0ull;
    GPIOIntClear(GPIO_PORTA_BASE,GPIO_INT_PIN_4);



    const uint32_t calibration1 = spiReadReg24(TDC7200_REG_ADR_CALIBRATION1);               // Retrieveing the CALIB1 value
    const uint32_t calibration2 = spiReadReg24(TDC7200_REG_ADR_CALIBRATION2);               // // Retrieveing the CALIB2 value
    const uint32_t time1= spiReadReg24(TDC7200_REG_ADR_TIME1);
    const uint32_t time2= spiReadReg24(TDC7200_REG_ADR_TIME2);
    const uint32_t clock_count1= spiReadReg24(TDC7200_REG_ADR_CLOCK_COUNT1);
    calCount= (((int64_t)(calibration2-calibration1)<<(shift))/(TDC7200_cal2_Period-1));          // Define calCount
    normLSB =((int64_t)(((PS_PER_SEC)/(TDC7200_CLOCK_FREQ)))<<(2*shift))/calCount;
    tof = (((int64_t)(time1)-(int64_t)(time2))*normLSB)>>shift;   //reg values
    tof+= ((uint64_t)clock_count1)*(uint64_t)((PS_PER_SEC)/(TDC7200_CLOCK_FREQ));

    spiWriteReg8(TDC7200_REG_ADR_CONFIG1,TDC7200_REG_SHIFT_CONFIG1_MEAS_MODE_2
                 |TDC7200_REG_SHIFT_FORCE_CAL|TDC7200_REG_SHIFT_INT_MASK_NEW_MEAS_MASK);  //Start the measurement

    if (tof == 0ull)
        {
        UARTprintf("0\n");
        SysCtlDelay(SysCtlClockGet() / (10 * 3));
        }

    else {
    char buff[40];
    ui64toa(tof, buff,10);
    UARTprintf("%s\n",buff);
    }


}


uint32_t spiReadReg24(const uint8_t addr)
{
    uint32_t val=0, temp=0;

        GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, 0);// Enabled SSI
    SSIDataPut(SSI2_BASE, ((addr & TDC7200_SPI_REG_ADDR_MASK)|TDC7200_SPI_REG_READ));
    SSIDataGet(SSI2_BASE, &junk);
    junk&= 0x00FF;
    SSIDataPut(SSI2_BASE,0u);
    SSIDataGet(SSI2_BASE, &temp);
    temp&= 0x000000FF;
    val=temp;
    val <<=8;
    SSIDataPut(SSI2_BASE,0u);
    SSIDataGet(SSI2_BASE, &temp);
    temp&= 0x000000FF;
    val|=temp;
    val <<=8;
    SSIDataPut(SSI2_BASE,0u);
    SSIDataGet(SSI2_BASE, &temp);
    temp&= 0x000000FF;
    val|=temp;
    GPIOPinWrite(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_PIN_0);// Enabled SSI

    return val;

}
static void ui64toa(uint64_t v, char * buf, uint8_t base)
{
  int idx = 0;
  int i,j;
  uint64_t w = 0;
  while (v > 0)
  {
    w = v / base;
    buf[idx++] = (v - w * base) + '0';
    v = w;
  }
  buf[idx] = 0;
  // reverse char array
  for (i = 0, j = idx - 1; i < idx / 2; i++, j--)
  {
    char c = buf[i];
    buf[i] = buf[j];
    buf[j] = c;
  }
}


