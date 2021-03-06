/* from Max & Luka */

#include "../h/pmc.h"
#include "../h/tc.h"
#include "../h/pio.h"
#include "../h/aic.h"

#define TC4_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE
#define TC5_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE | TC_ABETRG_TIOA
// ABETRG: TIOA or TIOB External Trigger Selection. When it is not set, then TIOB is the trigger

StructPIO* piobaseB   = PIOB_BASE;	// Basisadresse PIO B
StructPIO* piobaseA   = PIOA_BASE;
StructAIC* aicbase  = AIC_BASE;      
StructPMC* pmcbase = PMC_BASE;

int bechergewicht = 0, fuellgewicht = 0;
const int Maxgewicht = 50;
const int C1 =18030, C2 =40; // Waagenkonstanten
const int zapftoleranz = 10;
const int becher_minimum = 5;
const int becher_maximum = 500;
int currentlyPumping = 0; // "boolean" condition variable
int gewichttmp = 0;

void taste_irq_handler (void) __attribute__ ((interrupt));

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

void printInt(int number){
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

	StructTC* tcbase4 = TCB4_BASE;
	StructTC* tcbase5 = TCB5_BASE;

	/* Periodendauer der Waagensignale messen
	Signal aud TIOA4 ca. 16kHz entspricht ca. einer Periodendauer von 62,5us
	durch den Teiler von 32 ergeben sich ca. 2ms
	Zaehler mit positiver Flanke starten */

	// timer werden gestartet
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
	capturediff1   =   abs(captureRB1) - abs(captureRA1);
	Periodendauer1 = abs(capturediff1) / 12.5;  // Zeit in us

	m = (C1* ( (Periodendauer1/Periodendauer2) -1) -C2) + 0.5; // +0.5 for rounding before typecast

	if(m < 0) { //eliminate "extreme" inaccuracy by calling this function again
		m = MessungderMasse();		
	}
	return m;
}

int messen(void){
	int m = MessungderMasse();
	int n = MessungderMasse(); 
	while(m != n){ // waiting for "stability". This could lead to an endless loop
	m = MessungderMasse();
	n = MessungderMasse();       
	} 
	if(n < 0)
	  n = messen();
	// alternative way:
	//n = (n+m) / 2;
	return n;
}

inline void pumpeAn(){
	piobaseA->PIO_PDR  = (1<<PIOTIOA3) ;   // output port for Timer 3 enabled --> Pumpe An.
	currentlyPumping = 1;
}

inline void pumpeAus(){
	piobaseA->PIO_PER  = (1<<PIOTIOA3);    // output port for timer 3 disabled --> Pumpe Aus.
	currentlyPumping = 0;
	puts("Pumpen Beendet!\n");
}

inline void tarieren(){
	int bechertemp = messen(); // für if-Abfragen
	if(currentlyPumping == 0)
	{
		if ((bechertemp <=  becher_minimum) || (bechertemp > becher_maximum)){
			puts("Das ist wahrscheinlich kein leerer Becher\n");
			bechergewicht = 0;
		}
		else {
			bechergewicht = messen();
			puts("Gewicht des Bechers ist ");
			printInt(bechergewicht);
			puts(" Gramm. Waage ist tariert.\n");
			gewichttmp = 0;	    
		}
	}
	else
		puts("Es wird gerade gepumpt.\n");	
}



// ******************************* Interruptservice routine for Keys SW1 und SW2 *******************************
void taste_irq_handler (void) {  
	if (!(piobaseB->PIO_PDSR & KEY1)){      // if button 1 is pushed. Tarieren
		tarieren();
	}

	if (!(piobaseB->PIO_PDSR & KEY2)){ // wenn Taster 2 gedrueckt wird. PDSR wird mit KEY2 verglichen
		if (bechergewicht !=0)
		{
			pumpeAn();
			pumpe();
		}
	}
	aicbase->AIC_EOICR = piobaseB->PIO_ISR;		//confirm execution of interrupt 
}

// Timer3 initialisieren // fuer die Pumpe
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
	timerbase3->TC_RA = 31250; // Muss die Haelfte der Periodendauer sein   244
	timerbase3->TC_RC = 62500; // (25000000 Hz / Verlangsamer ) / 50 Hz = gesamte Periodendauer. 488 Groessere Werte aus irgendeinem Grund besser.

	// Start the timer :
	timerbase3->TC_CCR = TC_CLKEN ;                //__Einschalten der clk
	timerbase3->TC_CCR = TC_SWTRG ;                //__Einschalten des counters
	piobaseA->PIO_PER  = (1<<PIOTIOA3) ; // Port wird Enabled
	piobaseA->PIO_OER  = (1<<PIOTIOA3) ; // Port wird als Output gesetzt
	piobaseA->PIO_CODR = (1<<PIOTIOA3) ; // Output wird gecleart
}

void pumpe(void)
{
	fuellgewicht = 0;
	gewichttmp = 0;
	int tst = 1;
	while((fuellgewicht < Maxgewicht) && tst == 1)
	{ //loop for pumping:      
		gewichttmp = fuellgewicht;
		fuellgewicht = (messen()  - bechergewicht);
		//puts("Fuellgewicht: ");	// this prints the weight continously.
		//printInt(fuellgewicht);		// helpful for debugging.
		//puts("\n");
		if(gewichttmp > (fuellgewicht + zapftoleranz)){ //zapftoleranz, damit wir warten bis ein ausreichend grosser abstand zwischen Fuellgewicht und gewichttmp entsteht
			tst = 0;
			puts ("Ploetzlicher Gewichtsverlust! Wurde der Becher entfernt?\n");
		}

	}
	// always print how much stuff has been pumped
	puts("Es wurden ");
	// printInt(fuellgewicht);
	printInt (messen()-bechergewicht);
	puts(" Gramm abgefuellt!\n");

	pumpeAus();

	fuellgewicht = 0;
	gewichttmp = 0;
	bechergewicht = 0;
	tst = 1;
	int i = 0;
	while(i < 10000) // kleine Zeitpause, damit sich die Waage etwas "beruhigt"
	  i++;
}

void initialisation() {
	pmcbase->PMC_PCER = 0x4200;   // enable Periphal clocks for PIOB (0x4000) and TC3 (Timer3, 0x0200)
	pmcbase->PMC_PCER = 0x06e00;  // Clock PIOA, PIOB, Timer5, Timer4 einschalten

	// Initializing the Interrupt thingy
	aicbase->AIC_IDCR = 1<<14;
	aicbase->AIC_ICCR = 1<<14;
	aicbase->AIC_SVR[PIOB_ID] = (unsigned int) taste_irq_handler;
	aicbase->AIC_SMR[PIOB_ID] = 0x7;
	aicbase->AIC_IECR = 1<<14;
	piobaseB->PIO_IER =  KEY1 | KEY2;
	piobaseB->PIO_PER =  KEY1 | KEY2;

	inits();			//initialize serial port, via software interrupt. in ser_io.S
	// init_ser();		//in seriell.c. redundant because inits calls this already
	Timer3_init();
}

void greeting(void) {
	puts("Hallo und Tach!\n");
	puts("Waagenkonstanten sind C1 = ");
	printInt(C1);
	puts(" und C2 = ");
	printInt(C2);
	puts("\n");
	puts("Druecke Key1 fuer Kalibrierung. Key2 fuer zapfen.\n");
}  

void run(){
	greeting();
	while (currentlyPumping == 0) {
	  //puts ("in schleife \n");
	}
	currentlyPumping = 0;
}

int main(void)
{
	initialisation();
	run();
	return 0;
}
