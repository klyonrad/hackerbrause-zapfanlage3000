#include "../h/pmc.h"
#include "../h/tc.h"
#include "../h/pio.h"
#include "../h/aic.h"
 
#define TC4_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE
// next line by luka:
#define TC5_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE | TC_ABETRG_TIOA
// ABETRG: TIOA or TIOB External Trigger Selection. When it is not set, then TIOB is the trigger
 
 
int bechergewicht = 0, fuellgewicht = 0;
const int Maxgewicht = 50;
const int C1 =2000, C2 =0; // Waagenkonstanten
const int zapftoleranz = 5;
const int becher_minimum = 5;
const int becher_maximum = 35;
int pumpen = 0;
int gewichttmp = 0;
 
void taste_irq_handler (void) __attribute__ ((interrupt));
int main();

 
void intToChar(int value, char *buffer){
    int foo, length;
    int i = 0;
    if (value == 0){
        buffer[i++] = '0';
        buffer[i] = 0;
    }
    if (value > 0){
        while (value != 0) // stripping the digits
        {
            foo = value % 10;
            value = value / 10;
            buffer[i++] = '0' + foo;
        }
        length = i-1;
        int k = length;
        i = 0;
        while (i < k) // now we reverse the buffer array
        {
            buffer[i] ^= buffer[k];
            buffer[k] ^= buffer[i];
            buffer[i] ^= buffer[k];
            i++;
            k--;
        }
        buffer[length+1] = 0;
    }
    if (value < 0) // doing almost the same for negative numbers
    {
        buffer[i++] = '-';
        value = value * -1;
        while (value != 0)
        {
            foo = value % 10;
            value = value / 10;
            buffer[i++] = '0' + foo;
        }
                length = i-1;
        int k = length;
    i = 1;
        while (i < k)
        {
            buffer[i] ^= buffer[k];
            buffer[k] ^= buffer[i];
            buffer[i] ^= buffer[k];
            i++;
            k--;
        }
        buffer[length+1] = 0;
    }
    // ToDo: conversion for the "highest" negative integer is missing   
}
   
void intOutput(int number){
    char buffer[12]; // biggest 32bit integer has 11 digits; one more for '\0'
    intToChar (number, buffer);
    puts(buffer);
}
 
int MessungderMasse()
{
int m =0, sum = 0;
    volatile int    captureRA1;
    volatile int    captureRB1;
    volatile int    capturediff1;
    volatile int    capturediff2;
    volatile float Periodendauer1;
    volatile float Periodendauer2;
     
    StructPMC* pmcbase = PMC_BASE;
    StructPIO* piobaseA = PIOA_BASE;
    StructPIO* piobaseB = PIOB_BASE;
    StructTC* tcbase4 = TCB4_BASE;
    StructTC* tcbase5 = TCB5_BASE;
     
//  pmcbase->PMC_PCER = 0x06f80; // Clock PIOA, PIOB, Timer5, Timer4, Timer1 einschalten
//  pmcbase->PMC_PCER = 0x06c00; // Clock PIOA, PIOB, Timer5, Timer4 einschalten
     
// Periodendauer der Waagensignale messen
// Signal aud TIOA4 ca. 16kHz entspricht ca. einer Periodendauer von 62,5us
// durch den Teiler von 32 ergeben sich ca. 2ms
// Zaehler mit positiver Flanke starten
 
    piobaseA->PIO_PDR    =   0x090;      // Disables PIO control (enables peripheral control) on P4 and P7
    tcbase4->TC_CCR      =   TC_CLKDIS;
    tcbase4->TC_CMR      =   TC4_INIT;
    tcbase4->TC_CCR      =   TC_CLKEN;
    tcbase4->TC_CCR      =   TC_SWTRG;
 
    piobaseB->PIO_PDR    =   0x090;      // Disables PIO control (enables peripheral control) on P4 and P7
    tcbase5->TC_CCR      =   TC_CLKDIS;
    tcbase5->TC_CMR      =   TC5_INIT;
    tcbase5->TC_CCR      =   TC_CLKEN;
    tcbase5->TC_CCR      =   TC_SWTRG;
     
    // TC4 is getting bigger.
        tcbase5->TC_CCR  =   TC_SWTRG;
        while (!( tcbase5->TC_SR & TC_LDRBS));       // Capture Register B wurde geladen Messung abgeschlossen (1 Zyklus
            // 0x40 = LDRBS "RB Load has occurred since the last read of the Status Register, if WAVE = 0"
            captureRA1  = tcbase5->TC_RA;                // 
            captureRB1  = tcbase5->TC_RB;
            capturediff2    =   abs(captureRB1) - abs(captureRA1);
            Periodendauer2 = abs(capturediff2) / 12.5;  // Zeit in us
             
        tcbase4->TC_CCR  =   TC_SWTRG;
        while (!( tcbase4->TC_SR & TC_LDRBS));       // Capture Register B wurde geladen Messung abgeschlossen (1 Zyklus
            // 0x40 = LDRBS "RB Load has occurred since the last read of the Status Register, if WAVE = 0"
            captureRA1  = tcbase4->TC_RA;                // 
            captureRB1  = tcbase4->TC_RB;
            capturediff1    =   abs(captureRB1) - abs(captureRA1);
            Periodendauer1 = abs(capturediff1) / 12.5;  // Zeit in us
 
    m = (C1* ( (Periodendauer2/Periodendauer1) -1) -C2) + 0.5; // +0.5 for rounding before typecast
 
    if(m < 0) {//eliminate "extreme" inaccuracy by calling this function again
        MessungderMasse(); 
    }
    return m;
}
 
int messer(void){
    int m = MessungderMasse();
    int n = MessungderMasse(); 
    /*while(m != n){ // waiting for "stability". This could lead to an endless loop
        m = MessungderMasse();
        n = MessungderMasse();       
    } */
    // alternative way?:
    n = (n+m) / 2;
    return n;
}
 
// Interruptservice routine for Keys SW1 und SW2; for the pumping
void taste_irq_handler (void)
{
// annoying redundant(?) declaration of struct pointers
  StructPIO* piobaseB   = PIOB_BASE;                // Basisadresse PIO B
  StructPIO* piobaseA   = PIOA_BASE;
  StructAIC* aicbase  = AIC_BASE;                  
 
// ****************void taste_irq_handler (void) __attribute__ ((interrupt));*********************************
    if (!(piobaseB->PIO_PDSR & KEY2)){               // wenn Taster 1 gedr?ckt wird. PDSR wird mit KEY2 verglichen
        if (bechergewicht !=0){
        //Pumpe Starten
        piobaseA->PIO_PDR  = (1<<PIOTIOA3) ;   //_ output port for Timer 3 enabled *** pumpen start ***
	pumpen = 1;
        }
        else
          aicbase->AIC_EOICR = piobaseB->PIO_ISR;           //__Rueckgabe das interrupt ausgef?hrt wurde
    }
 
    else if (!(piobaseB->PIO_PDSR & KEY1)){            // if button 2 is pushed, does 0x100 really mean "pressed" ?
        //piobaseA->PIO_PDR  = (1<<PIOTIOA3) ; //_ output port for Timer 3 enabled *** pumpen start ***
        //Messen des Bechers
        
        int bechertemp = messer(); // für if-Abfragen
        if(pumpen == 0){
			if ((bechertemp <=  becher_minimum) || (bechertemp > becher_maximum)){
				puts("Das ist wahrscheinlich kein leerer Becher\n");
				bechergewicht = 0;
			// ToDo: start from the beginning
			}
			else {     
				bechergewicht = bechertemp;
				puts("Gewicht des Bechers ist ");
				intOutput(bechergewicht);
				puts(" Gramm. Waage ist tariert.\n");
				gewichttmp = 0;
				
			}
	}else{
		puts("Es wird bereits gepumpt");
	}
	
    }
    aicbase->AIC_EOICR = piobaseB->PIO_ISR;           //__Rueckgabe das interrupt ausgef?hrt wurde 
    
  }

// Timer3 initialisieren // f?r die Pumpe
void Timer3_init( void )
{
    StructTC* timerbase3  = TCB3_BASE;      // Basisadressse TC Block 1
    StructPIO* piobaseA   = PIOA_BASE;      // Basisadresse PIO B
 
    timerbase3->TC_CCR = TC_CLKDIS;          // Disable Clock
  
  // Initialize the mode of the timer 3
  timerbase3->TC_CMR =
    TC_ACPC_CLEAR_OUTPUT  |    //ACPC    : Register C clear TIOA
    TC_ACPA_SET_OUTPUT    |    //ACPA    : Register A set TIOA
    TC_WAVE               |    //WAVE    : Waveform mode
    TC_CPCTRG             |    //CPCTRG  : Register C compare trigger enable
    TC_CLKS_MCK8;           //TCCLKS  : MCKI / 1024 ---- es kfnnte sinnvoller sein, kleineren Verlangsamer zu benutzen fuer bessere Genauigkeit
 
  // Initialize the counter:
  timerbase3->TC_RA = 31250; //__ Muss die Haelfte der Periodendauer sein   244
  timerbase3->TC_RC = 62500; //__ (25000000 Hz / Verlangsamer ) / 50 Hz = gesamte Periodendauer. 488 Groessere Werte aus irgendeinem Grund besser.
 
  // Start the timer :
  timerbase3->TC_CCR = TC_CLKEN ;                //__Einschalten der clk
  timerbase3->TC_CCR = TC_SWTRG ;                //__Einschalten des counters
  piobaseA->PIO_PER  = (1<<PIOTIOA3) ; //__Der Port wird Enabled
  piobaseA->PIO_OER  = (1<<PIOTIOA3) ; //__Der Port wird als Output gesetzt
  piobaseA->PIO_CODR = (1<<PIOTIOA3) ; //__Output wird gecleart
}
 
void pumpe(void)
{
    StructPMC* pmcbase  = PMC_BASE;         // Basisadresse des PMC
    StructPIO* piobaseA     = PIOA_BASE;        // Basisadresse PIO A
    StructPIO* piobaseB     = PIOB_BASE;        // Basisadresse PIO B   
     
// ab hier entsprechend der Aufgabestellung ergdnzen
//**************************************************
    StructAIC* aicbase = AIC_BASE; // Initializing the Interrupt thingy
    aicbase->AIC_IDCR = 1<< 14;
    aicbase->AIC_ICCR = 1<<14;
    aicbase->AIC_SVR[PIOB_ID] = (unsigned int) taste_irq_handler;
    aicbase->AIC_SMR[PIOB_ID] = 0x7;
    aicbase->AIC_IECR = 1<<14;
    piobaseB->PIO_IER =  KEY1 | KEY2;
    piobaseB->PIO_PER =  KEY1 | KEY2;
 
    // ToDo: only pump when we calibrated bechergewicht!

    int tst = 1;
    while((fuellgewicht <= Maxgewicht) && tst == 1)
    { //loop for pumping:      
        gewichttmp = fuellgewicht;
        fuellgewicht = (messer() - bechergewicht);
        //puts("Fuellgewicht: ");	//Sparen von unnoetigen ausgaben des aktuellen gewichts
        intOutput(fuellgewicht);	// might be helpful for debugging.
        puts("\n");
         
        if(gewichttmp > (fuellgewicht + zapftoleranz)){ //damit wir warten bis ein ausreichend grosser abstand zwischen Fuellgewicht und gewichttmp entsteht
          tst = 0;
          puts ("Ploetzlicher Gewichtsverlust! Wurde der Becher entfernt?\n");
        }
 
    }
    //if(tst == 1){
	puts("Es wurden ");
	intOutput(fuellgewicht);
	puts(" Gramm abgefuellt!\n");
    //}
    piobaseA->PIO_PER  = (1<<PIOTIOA3);    //__output port for timer 3 disabled
    pumpen = 0;
    puts("Pumpen Beendet!\n");
    //aicbase->AIC_IDCR = 1<<14;
    //aicbase->AIC_ICCR = 1<<14;
    fuellgewicht = 0;
    gewichttmp = 0;
    bechergewicht = 0;
    tst = 1;
    int sdfk = main();						//Zurueck an den anfang und alles Nochmal machen
}
 
void greeting(void) {
    puts("Hallo und Tach!\n");
    puts("Waagenkonstanten sind C1 = ");
    intOutput(C1);
    puts(" und C2 = ");
    intOutput(C2);
    puts("\n");
    puts("Druecke Key1 fuer Kalibrierung. Key2 fuer zapfen.\n");
}  
 void taste1(){
    StructPIO* piobaseB = PIOB_BASE;
    while((piobaseB->PIO_PDSR & KEY1)){}
    int bechertemp = messer(); // für if-Abfragen
        if(pumpen == 0){
			if ((bechertemp <=  becher_minimum) || (bechertemp > becher_maximum)){
				puts("Das ist wahrscheinlich kein leerer Becher\n");
				bechergewicht = 0;
			// ToDo: start from the beginning
			}
			else {     
				bechergewicht = bechertemp;
				puts("Gewicht des Bechers ist ");
				intOutput(bechergewicht);
				puts(" Gramm. Waage ist tariert.\n");
				gewichttmp = 0;
				
			}
	}else{
		puts("Es wird bereits gepumpt");
	}
 }
 
int main(void)
{
    StructPMC* pmcbase = PMC_BASE;
    pmcbase->PMC_PCER    = 0x4200;   // enable Periphal clocks for PIOB (0x4000) and TC3 (Timer3, 0x0200)
    pmcbase->PMC_PCER = pmcbase->PMC_PCER | 0x06e00;  // Clock PIOA, PIOB, Timer5, Timer4 einschalten // das war vorher in messungdermasse
    StructPIO* piobaseB     = PIOB_BASE;
    
    //inits();			//Redundantes aufrufen der funktion init_ser()
    init_ser();			//initialisierung der seriellen schnittstelle
    Timer3_init();
    
    greeting();			//Ausgabe der Begruessung
    //taste1();
    pumpe();//Diese Methode verwaltet das pumpen. Gestartet wird das Pumen in der Interrupt Routine und beendet in der Methode pumpe()
 
 return 0;
}
