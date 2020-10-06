#define xdc__strict                       //gets rid of #303-D typedef warning re Uint16, Uint32
//#define abs(t) (((t)>=0) ? (t) : -(t))  // define absolute
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <xdc/std.h>
#include <ti/sysbios/BIOS.h>
#include "Peripheral_Headers/F2802x_Device.h"

//function prototypes:
/*
 *  ======== main ========
 */
Int main()
{
    DeviceInit(); //initialize peripherals
    /*
     * Start BIOS
     * Perform a few final initializations and then
     * fall into a loop that continually calls the
     * installed Idle functions.
     */
    BIOS_start();    /* does not return */
    return(0);
}

/*
 *  ======== myTickFxn ========
 *  Timer Tick function that increments a counter, and sets the isrFlag.
 */
Void myTickFxn(UArg arg)
{
    GpioDataRegs.GPASET.bit.GPIO2 = 1;    // set GPIO 2 to High to get CPU utilization
    tickCount += 1;    /* increment the counter */

}

/*
 *  ======== myIdleFxn ========
 *  Background idle function that is called repeatedly
 *  from within BIOS_start() thread.
 */
Void myIdleFxn(Void)
{
    // we dont really need idle, so our task usage could be 100%
    // bottom is the setup pin if later we want to use Idle for a LED
    GpioDataRegs.GPACLEAR.bit.GPIO2 = 1;    // set GPIO 2 to Low to get CPU utilization
}
/*
 *  ======== myPBFxn ========
 *  Reset Button to a center position for Solar Panel
 */
Void myPBFxn(Void)
{
    GpioDataRegs.GPASET.bit.GPIO2 = 1;    // set GPIO 2 to High to get CPU utilization
    pbcount++;
    EPwm1Regs.CMPA.half.CMPA = 176;      // center point of the system
}

/*
 *  ======== AtoD ========
 *  Hwi1: hardware interrupt
 *  Analog to Digital converter based on timer (period 100us)
 *  read ADC value from photo resistor
 */
Void AtoD(Void)
{
    GpioDataRegs.GPASET.bit.GPIO2 = 1;    // set GPIO 2 to High to get CPU utilization
    AdcRegs.ADCSOCFRC1.all = 0x1; //start conversion via software
    while (AdcRegs.ADCINTFLG.bit.ADCINT1 == 0)
    {
        ; //wait for interrupt flag to be set
    }
    AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; //clear interrupt flag

    PhotoL = AdcResult.ADCRESULT0; //get reading ch J5 5
    PhotoR = AdcResult.ADCRESULT1; //get reading ch J5 7 small Voltage darker

    Swi_post(swi0);
}

/*
 *  ======== Comp ========
 *  Swi0: software interrupt
 *  Analog to Digital converter based on timer (period 100us)
 *  read ADC value from photo resistor
 */
Void Comp(UArg arg)
{
    GpioDataRegs.GPASET.bit.GPIO2 = 1;    // set GPIO 2 to High to get CPU utilization
    tickCount += 1;
    Percent_PhotoL = 100 * PhotoL / 420;
    Percent_PhotoR = 100 * PhotoR / 420;

    // test_v = abs(7-8);
    // post two values for direction moving purpose
    Semaphore_post(DirectionSem);
}

void DirectionCalcTask(void)
{
    /*
     * Pend on "mySemaphore" until the timer ISR says
     * its time to do something.
     */
    for (;;) {
        GpioDataRegs.GPASET.bit.GPIO2 = 1;    // set GPIO 2 to High to get CPU utilization
        Semaphore_pend(DirectionSem, BIOS_WAIT_FOREVER);
        /* Light side has the smaller resistance
                   normal running @ Capstone room: based on observation
                   RangeR: 240-480
                   PhotoR :290 -320*/
                   // Move right: max (258) @ cw (0.6 - 1.5ms); 2/480 *100% < 1%

        if ((Percent_PhotoR < Percent_PhotoL) && (Percent_PhotoR < 1)) {
            if (EPwm1Regs.CMPA.half.CMPA < 258) {
                EPwm1Regs.CMPA.half.CMPA++;
            }
            //EPwm1Regs.CMPA.half.CMPA += abs(Percent_PhotoR-0.5)*100/(50/29); // pass dutyCycle() here
        }
        /*
         RangeL: 0 - 420
         PhotoL: 21- 67 */
         // Move left: min (94) @ cw (1.5 - 2.2ms);6/350*100% = 0 <5%
        else if ((Percent_PhotoL < Percent_PhotoR) && (Percent_PhotoL < 5)) {

            if (EPwm1Regs.CMPA.half.CMPA > 94) {
                EPwm1Regs.CMPA.half.CMPA--;
            }
        }

        else {
            //EPwm1Regs.CMPA;// pass dutyCycle()
            //EPwm1Regs.CMPA.half.CMPA=176;
        }
        //Semaphore_pend(DirectionSem, BIOS_WAIT_FOREVER);
        EALLOW;
        // reading 1 - respective grp sent to CPU & other interrupts current blocked
        PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
        EDIS;

    }
}
#include "Peripheral_Headers/F2802x_Device.h"
//function prototypes:
extern void DelayUs(Uint16);

void DeviceInit(void);

//---------------------------------------------------------------
//  Configure Device for target Application Here
//---------------------------------------------------------------
void DeviceInit(void)
{
    EALLOW; // temporarily unprotect registers

 // LOW SPEED CLOCKS prescale register settings
    SysCtrlRegs.LOSPCP.all = 0x0002; // Sysclk / 4 (15 MHz)
    SysCtrlRegs.XCLK.bit.XCLKOUTDIV = 2;

    // PERIPHERAL CLOCK ENABLES 
    //---------------------------------------------------
    // If you are not using a peripheral you may want to switch
    // the clock off to save power, i.e., set to =0 
    // 
    // Note: not all peripherals are available on all 280x derivates.
    // Refer to the datasheet for your particular device. 

    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;    // ADC
    //------------------------------------------------
    SysCtrlRegs.PCLKCR3.bit.COMP1ENCLK = 0;	// COMP1
    SysCtrlRegs.PCLKCR3.bit.COMP2ENCLK = 0;	// COMP2
    //------------------------------------------------
    SysCtrlRegs.PCLKCR0.bit.I2CAENCLK = 0;   // I2C
    //------------------------------------------------
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 0;	// SPI-A
    //------------------------------------------------
    SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 0;  	// SCI-A
    //------------------------------------------------
    SysCtrlRegs.PCLKCR1.bit.ECAP1ENCLK = 0;	// eCAP1
    //------------------------------------------------
    SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;  // ePWM1
    SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 0;  // ePWM2
    SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 0;  // ePWM3
    SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 0;  // ePWM4
    //------------------------------------------------
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;   // Enable TBCLK
    //------------------------------------------------
    //---------------------------------------------------------------
    // GPIO (GENERAL PURPOSE I/O) CONFIG
    //---------------------------------------------------------------
    //-----------------------
    // QUICK NOTES on USAGE:
    //-----------------------
    // If GpioCtrlRegs.GP?MUX?bit.GPIO?= 1, 2, or 3 (i.e., Non GPIO func), then leave rest of lines commented
// If GpioCtrlRegs.GP?MUX?bit.GPIO?= 0 (i.e., GPIO func), then:
//	1) uncomment GpioCtrlRegs.GP?DIR.bit.GPIO? = ? and choose pin to be IN or OUT
//	2) If IN, can leave next two lines commented
//	3) If OUT, uncomment line with ..GPACLEAR.. to force pin LOW or
//       uncomment line with ..GPASET.. to force pin HIGH or
//---------------------------------------------------------------
//---------------------------------------------------------------
//  GPIO-00 - PIN FUNCTION = blue LED D2 (rightmost on LaunchPad)
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1; // 0=GPIO,  1=EPWM1A,  2=Resv,  3=Resv
    GpioCtrlRegs.GPADIR.bit.GPIO0 = 1; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO0 = 1; // uncomment if --> Set Low initially
    GpioDataRegs.GPASET.bit.GPIO0 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-01 - PIN FUNCTION = blue LED D4
    GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 0; // 0=GPIO,  1=EPWM1B,  2=EMU0,  3=COMP1OUT
    GpioCtrlRegs.GPADIR.bit.GPIO1 = 1; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO1 = 1; // uncomment if --> Set Low initially
    GpioDataRegs.GPASET.bit.GPIO1 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-02 - PIN FUNCTION = blue LED D3
    GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 0; // 0=GPIO,  1=EPWM2A,  2=Resv,  3=Resv
    GpioCtrlRegs.GPADIR.bit.GPIO2 = 1; // 1=OUTput,  0=INput 
    GpioDataRegs.GPACLEAR.bit.GPIO2 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO2 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-03 - PIN FUNCTION = blue LED D5 (leftmost on LaunchPad)
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 0; // 0=GPIO,  1=EPWM2B,  2=Resv,  3=COMP2OUT
    GpioCtrlRegs.GPADIR.bit.GPIO3 = 1; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO3 = 1; // uncomment if --> Set Low initially
    GpioDataRegs.GPASET.bit.GPIO3 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-04 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 0; // 0=GPIO,  1=EPWM3A, 2=Resv, 	3=Resv
    GpioCtrlRegs.GPADIR.bit.GPIO4 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO4 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO4 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-05 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 0; // 0=GPIO,  1=EPWM3B,  2=Resv,  3=ECAP1
    GpioCtrlRegs.GPADIR.bit.GPIO5 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO5 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO5 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-06 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0; // 0=GPIO,  1=EPWM4A,  2=SYNCI,  3=SYNCO
    GpioCtrlRegs.GPADIR.bit.GPIO6 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO6 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO6 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-07 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 0; // 0=GPIO,  1=EPWM4B,  2=SCIRX-A,  3=Resv
    GpioCtrlRegs.GPADIR.bit.GPIO7 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO7 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO7 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-08 - GPIO-11 Do Not Exist
//---------------------------------------------------------------
//  GPIO-12 - PIN FUNCTION = Normally Open pushbutton S3 on LaunchPad (pulled-down)
    GpioCtrlRegs.GPAMUX1.bit.GPIO12 = 0; // 0=GPIO,  1=TZ1,  2=SCITX-A,  3=Resv
    GpioCtrlRegs.GPADIR.bit.GPIO12 = 0; // 1=OUTput,  0=INput
//  GpioDataRegs.GPACLEAR.bit.GPIO12 = 1;   // uncomment if --> Set Low initially
//  GpioDataRegs.GPASET.bit.GPIO12 = 1; // uncomment if --> Set High initially
    GpioCtrlRegs.GPAPUD.bit.GPIO12 = 1; //disable internal pull-up resistor
    GpioIntRegs.GPIOXINT1SEL.bit.GPIOSEL = 12;
    XIntruptRegs.XINT1CR.bit.POLARITY = 1;
    XIntruptRegs.XINT1CR.bit.ENABLE = 1;
    //---------------------------------------------------------------
    //  GPIO-13 - GPIO-15 = Do Not Exist
    //---------------------------------------------------------------
    //---------------------------------------------------------------

    //  GPIO-16 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 0; // 0=GPIO,  1=SPISIMO-A,  2=Resv,  3=TZ2
    GpioCtrlRegs.GPADIR.bit.GPIO16 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO16 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO16 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-17 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 0; // 0=GPIO,  1=SPISOMI-A,  2=Resv,  3=TZ3
    GpioCtrlRegs.GPADIR.bit.GPIO17 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO17 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO17 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-18 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 0; // 0=GPIO,  1=SPICLK-A,  2=SCITX-A,  3=XCLKOUT
    GpioCtrlRegs.GPADIR.bit.GPIO18 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO18 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO18 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-19 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0; // 0=GPIO,  1=SPISTE-A,  2=SCIRX-A,  3=ECAP1
    GpioCtrlRegs.GPADIR.bit.GPIO19 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO19 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO19 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-20 - GPIO-27 = Do Not Exist
//---------------------------------------------------------------
//  GPIO-28 - PIN FUNCTION = --Spare-- (can connect to SCIRX on LaunchPad)
    GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 0; // 0=GPIO,  1=SCIRX-A,  2=I2C-SDA,  3=TZ2
//	GpioCtrlRegs.GPADIR.bit.GPIO28 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO28 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO28 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-29 - PIN FUNCTION = --Spare-- (can connect to SCITX on LaunchPad)
    GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 0; // 0=GPIO,  1=SCITXD-A,  2=I2C-SCL,  3=TZ3
//	GpioCtrlRegs.GPADIR.bit.GPIO29 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPACLEAR.bit.GPIO29 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPASET.bit.GPIO29 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-30 - GPIO-31 = Do Not Exist
//---------------------------------------------------------------
//--------------------------------------------------------------

//  GPIO-32 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPBMUX1.bit.GPIO32 = 0; // 0=GPIO,  1=I2C-SDA,  2=SYNCI,  3=ADCSOCA
    GpioCtrlRegs.GPBDIR.bit.GPIO32 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPBSET.bit.GPIO32 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-33 - PIN FUNCTION = --Spare--
    GpioCtrlRegs.GPBMUX1.bit.GPIO33 = 0; // 0=GPIO,  1=I2C-SCL,  2=SYNCO,  3=ADCSOCB
    GpioCtrlRegs.GPBDIR.bit.GPIO33 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;	// uncomment if --> Set Low initially
//	GpioDataRegs.GPBSET.bit.GPIO33 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-34 - PIN FUNCTION = switch S1.1 on LaunchPad
    GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0; // 0=GPIO,  1=COMP2OUT,  2=EMU1,  3=Resv
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 0; // 1=OUTput,  0=INput 
//	GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1; // uncomment if --> Set Low initially
//	GpioDataRegs.GPBSET.bit.GPIO34 = 1; // uncomment if --> Set High initially
//---------------------------------------------------------------
//  GPIO-35 - GPIO-38 = Used for JTAG Port
//---------------------------------------------------------------
//---------------------------------------------------------------

//---------------------------------------------------------------
// INITIALIZE A-D & PWM
//---------------------------------------------------------------
//input channel = junction temperature sensor, SOC0, software triggering

    //simultaneously power up ADC's analog circuitry, bandgap, and reference buffer:
    AdcRegs.ADCCTL1.all = 0x00e0;

    //bit 7     ADCPWDN (ADC power down): 0=powered down, 1=powered up
    //bit 6     ADCBGPWD (ADC bandgap power down): 0=powered down, 1=powered up
    //bit 5     ADCREFPWD (ADC reference power down): 0=powered down, 1=powered up

    //generate INT pulse on end of conversion:
    AdcRegs.ADCCTL1.bit.INTPULSEPOS = 1;

    //enable ADC:
    AdcRegs.ADCCTL1.bit.ADCENABLE = 1;

    //wait 1 ms after power-up before using the ADC:
    DelayUs(1000);

    AdcRegs.ADCSAMPLEMODE.bit.SIMULEN0 = 1;

    //configure to sample on-chip temperature sensor:
   // AdcRegs.ADCCTL1.bit.TEMPCONV = 1; //connect A5 to temp sensor
    AdcRegs.ADCSOC0CTL.bit.CHSEL = 1;   //set SOC0 to sample A0, FOR ADCINA11 $ ADCINB1
    AdcRegs.ADCSOC0CTL.bit.ACQPS = 0x6; //set SOC0 window to 7 ADCCLKs
    //AdcRegs.INTSEL1N2.bit.INT1SEL = 0; //connect interrupt ADCINT1 to EOC0
    AdcRegs.INTSEL1N2.bit.INT1SEL = 0; //
    AdcRegs.INTSEL1N2.bit.INT1CONT = 0; //
    AdcRegs.INTSEL1N2.bit.INT1E = 1; //enable interrupt ADCINT1
// EPWM1 Initialization
    EPwm1Regs.TBPRD = 2344;              // Period in TBCLK counts, sets period based on SYSCLK(60MHz) / (CLKDIV*HSPCLKDIV)
    EPwm1Regs.CMPA.half.CMPA = 176;     // Compare counter for EPWMA, sets duty cycle relate to center of the solar Panel
        //EPwm1Regs.CMPA = 45;
    EPwm1Regs.CMPB = 200;               // Compare counter for EPWMB, sets duty cycle
    EPwm1Regs.TBPHS.all = 0;            // Set Phase register to zero
    EPwm1Regs.TBCTR = 0;                // clear TBCLK counter
    EPwm1Regs.TBCTL.bit.CTRMODE = 0;    // Set mode to up-count
    EPwm1Regs.TBCTL.bit.PHSEN = 0;      // Phase loading disabled
    EPwm1Regs.TBCTL.bit.PRDLD = 0;      // Select Shadow Register Loading
    EPwm1Regs.TBCTL.bit.SYNCOSEL = 3;   // Disables the EPWM Synchronization
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 2;  // Multiplies SYSCLKDIV divisor CLKDIV
    EPwm1Regs.TBCTL.bit.CLKDIV = 7;     // Determines SYSCLK Divisor to create TBCLK
    EPwm1Regs.CMPCTL.bit.SHDWAMODE = 0; // Select Counter Compare Shadow Mode for EPWMA
    EPwm1Regs.CMPCTL.bit.SHDWBMODE = 0; // Select Counter Compare Shadow Mode for EPWMB
    EPwm1Regs.CMPCTL.bit.LOADAMODE = 0; // Set timer to zero initially for EPWMA
    EPwm1Regs.CMPCTL.bit.LOADBMODE = 0; // Set timer to zero initially for EPWMB
    EPwm1Regs.AQCTLA.bit.ZRO = 2;       // set for EPWMA
    EPwm1Regs.AQCTLA.bit.CAU = 1;       // Clear for EPWMA; Set output low when counter = Compare Counter for EPWMA
    EPwm1Regs.AQCTLB.bit.ZRO = 2;       // Set output high when counter = 0 for EPWMB
    EPwm1Regs.AQCTLB.bit.CBU = 1;       // Set output low when counter = Compare Counter for EPWMB
    /* for sawtooth pulse set up
        EPwm1Regs.AQCTLA.bit.ZRO = 2;       // set for EPWMA
        EPwm1Regs.AQCTLA.bit.PRD = 0;       // Do nothing for EPWMA
        EPwm1Regs.AQCTLA.bit.CAU = 1;       // Clear for EPWMA; Set output low when counter = Compare Counter for EPWMA
        EPwm1Regs.AQCTLB.bit.ZRO = 2;       // Set output high when counter = 0 for EPWMB
        EPwm1Regs.AQCTLB.bit.CBU = 0;       // Set output low when counter = Compare Counter for EPWMB
        EPwm1Regs.AQCTLB.bit.CBD = 0;       // Do nothing
    //Run Time
    // = = = = = = = = = = = = = = = = = = = = = = = =
    EPwm1Regs.CMPA.half.CMPA = Duty1A;  // adjust duty for output EPWM1A
    EPwm1Regs.CMPB = Duty1B;            // adjust duty for output EPWM1B
    */

    //    // Initialize EPWM1A pin GPIO0. Row J6 pin 1
    //    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;     // 0=GPIO,  1=EPWM1B
    //    GpioCtrlRegs.GPADIR.bit.GPIO0 = 1;      // 1=OUTput,  0=INput
    EDIS;	// restore protection of registers


} // end DeviceInit()

//===============================================================
// End of file
//===============================================================



