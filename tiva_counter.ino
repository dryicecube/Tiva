#include <stdint.h>
#include <stdbool.h>
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"

unsigned long prevT = 0;
unsigned long counts = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  configure_GPIO();                                                   //config GPIO pins
}

void loop() {
  // put your main code here, to run repeatedly: 
    unsigned long currT = millis();
  if (currT - prevT >= 1000){   // Display the counts after a 1000 millisecond period.
    Serial.println(counts);
    counts = 0;
    prevT = currT;
    }
  
}

void configure_GPIO(void)                                                                          // Configuring GPIO Pins
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);                                                  // Port F Enabled
    GPIOPinTypeGPIOInput(GPIO_PORTD_BASE, GPIO_PIN_2);                                            // PF4 Set As Input 
    GPIOPadConfigSet(GPIO_PORTD_BASE, GPIO_PIN_2,GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);                           
    GPIOIntTypeSet(GPIO_PORTD_BASE,GPIO_PIN_2 ,GPIO_RISING_EDGE);                                 // Setting Interrupt Condition For PF4 as Rising Edge
    GPIOIntRegister(GPIO_PORTD_BASE, SPD_Handdler);                                              // Registering Interrupt With the Corresponding Function
    GPIOIntEnable(GPIO_PORTD_BASE, GPIO_INT_PIN_2);                                               // Interrupt Enable

}


void SPD_Handdler()
{
  GPIOIntClear(GPIO_PORTD_BASE,GPIO_INT_PIN_2);
  counts++;
}
