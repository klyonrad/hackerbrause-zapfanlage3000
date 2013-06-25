// Programmrahmen zur Aufgabe Termin5
// Aufgabe 1
//************************************
// 
// von: Manfred Pester
// vom: 06. August 2003
// letzte Änderung:	30. November 2004
// von: Manfred Pester
// #include "seriell.h"
// #include "swi.c"


#include "../h/pmc.h"
#include "../h/tc.h"
#include "../h/pio.h"
#include "../h/aic.h"

#define TC4_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE
// next line by luka:
#define TC5_INIT  TC_CLKS_MCK2 | TC_LDBSTOP | TC_CAPT | TC_LDRA_RISING_EDGE | TC_LDRB_RISING_EDGE | TC_ABETRG_TIOA
// ABETRG: TIOA or TIOB External Trigger Selection. When it is not set, then TIOB is the trigger


int bechergewicht, fuellgewicht = 0;
const int Maxgewicht = 50;

//static string begruessung ={"bla bla"};
//static string fehler ={"Hier die Fehlermeldung"};

void taste_irq_handler (void) __attribute__ ((interrupt));

int MessungderMasse()
{
int m =0, sum = 0;
	volatile int	captureRA1;
	volatile int	captureRB1;
	volatile int	capturediff1;
	volatile int 	capturediff2;
	volatile float Periodendauer1;
	volatile float Periodendauer2;
	
	StructPMC* pmcbase = PMC_BASE;
	StructPIO* piobaseA = PIOA_BASE;
	StructPIO* piobaseB = PIOB_BASE;
	StructTC* tcbase4 = TCB4_BASE;
	StructTC* tcbase5 = TCB5_BASE;
	
//	pmcbase->PMC_PCER = 0x06f80;	// Clock PIOA, PIOB, Timer5, Timer4, Timer1 einschalten
	pmcbase->PMC_PCER = pmcbase->PMC_PCER | 0x06c00;	// Clock PIOA, PIOB, Timer5, Timer4 einschalten

	
// Periodendauer der Waagensignale messen
// Signal aud TIOA4 ca. 16kHz entspricht ca. einer Periodendauer von 62,5us
// durch den Teiler von 32 ergeben sich ca. 2ms
// ZÃ¤hler mit positiver Flanke starten 

	piobaseA->PIO_PDR	=	0x090;		// Disables PIO control (enables peripheral control) on P4 and P7.Why disable?
	tcbase4->TC_CCR		=	TC_CLKDIS;
	tcbase4->TC_CMR		=	TC4_INIT;
	tcbase4->TC_CCR		=	TC_CLKEN;  // wouldn't it be more useful to do this one line later?
	tcbase4->TC_CCR		=	TC_SWTRG;

	piobaseB->PIO_PDR	=	0x090;		// Disables PIO control (enables peripheral control) on P4 and P7.Why disable?
	tcbase5->TC_CCR		=	TC_CLKDIS;
	tcbase5->TC_CMR		=	TC5_INIT;
	tcbase5->TC_CCR		=	TC_CLKEN;  // wouldn't it be more useful to do this one line later?
	tcbase5->TC_CCR		=	TC_SWTRG;
	//int i = 0;
	//while(i < 10){
	
	// TC4 is getting bigger.
		tcbase5->TC_CCR	=	TC_SWTRG;
		while (!( tcbase5->TC_SR & TC_LDRBS));		// Capture Register B wurde geladen Messung abgeschlossen (1 Zyklus
			// 0x40 = LDRBS "RB Load has occurred since the last read of the Status Register, if WAVE = 0"
			captureRA1	= tcbase5->TC_RA;				// 	
			captureRB1	= tcbase5->TC_RB;
			capturediff2	=	abs(captureRB1) - abs(captureRA1);
			Periodendauer2 = abs(capturediff2) / 12.5;	// Zeit in us
			
		tcbase4->TC_CCR	=	TC_SWTRG;
		while (!( tcbase4->TC_SR & TC_LDRBS));		// Capture Register B wurde geladen Messung abgeschlossen (1 Zyklus
			// 0x40 = LDRBS "RB Load has occurred since the last read of the Status Register, if WAVE = 0"
			captureRA1	= tcbase4->TC_RA;				// 	
			captureRB1	= tcbase4->TC_RB;
			capturediff1	=	abs(captureRB1) - abs(captureRA1);
			Periodendauer1 = abs(capturediff1) / 12.5;	// Zeit in us

// Aufgabe 3

	const int C1 =2000, C2 =0; // Waagenkonstanten
	
	m = (C1*((Periodendauer2/Periodendauer1)-1)-C2) + 0.5; // +0.5 for rounding before typecast

	//i++;
	//}
	//m /= i;
	if(m <0)
		MessungderMasse();
	
	return m;
}

int messer(void){
	int m = MessungderMasse();
	int n = MessungderMasse();
	
	while(m != n){
		m = MessungderMasse();
		n = MessungderMasse();	}
	return n;
	}



// Interruptserviceroutine für die Tasten SW1 und SW2; für die Pumpe
void taste_irq_handler (void)
{
// annoying redundant declaration of struct pointers
  StructPIO* piobaseB   = PIOB_BASE;				// Basisadresse PIO B
  StructPIO* piobaseA   = PIOA_BASE;
  StructAIC* aicbase  = AIC_BASE;					
	
// ab hier entsprechend der Aufgabestellung ergänzen
// ****************void taste_irq_handler (void) __attribute__ ((interrupt));*********************************
	if (!(piobaseB->PIO_PDSR & KEY2)){ 				// wenn Taster 1 gedrückt wird. PDSR wird mit KEY2 verglichen
		//piobaseA->PIO_PER  = (1<<PIOTIOA3) ;	//__output port for timer 3 disabled *** pumpen end ***
		
		//Pumpe Starten
		piobaseA->PIO_PDR  = (1<<PIOTIOA3) ;	//_ output port for Timer 3 enabled *** pumpen start ***
	
	}else if (!(piobaseB->PIO_PDSR & KEY1)){ 			// if button 2 is pushed, does 0x100 really mean "pressed" ?
		//piobaseA->PIO_PDR  = (1<<PIOTIOA3) ;	//_ output port for Timer 3 enabled *** pumpen start ***
		
		//ausgebe der Begrüßung und Messen sed Bechers
		puts("Hallo die Begruessung\n");
		bechergewicht = messer();
		
		//ausgebe des Gewichts
		
		
	}
	
	
	aicbase->AIC_EOICR = piobaseB->PIO_ISR;			//__Rückgabe das interrupt ausgeführt wurde
}

// Timer3 initialisieren // für die Pumpe
void Timer3_init( void )
{
  StructTC* timerbase3  = TCB3_BASE;		// Basisadressse TC Block 1
  StructPIO* piobaseA   = PIOA_BASE;		// Basisadresse PIO B

	timerbase3->TC_CCR = TC_CLKDIS;			// Disable Clock
 
  // Initialize the mode of the timer 3
  timerbase3->TC_CMR =
    TC_ACPC_CLEAR_OUTPUT  |    //ACPC    : Register C clear TIOA
    TC_ACPA_SET_OUTPUT    |    //ACPA    : Register A set TIOA
    TC_WAVE               |    //WAVE    : Waveform mode
    TC_CPCTRG             |    //CPCTRG  : Register C compare trigger enable
    TC_CLKS_MCK8;           //TCCLKS  : MCKI / 1024 ---- es könnte sinnvoller sein, kleineren Verlangsamer zu benutzen für bessere Genauigkeit

  // Initialize the counter:
  timerbase3->TC_RA = 31250;	//__ Muss die Hälfte der Periodendauer sein   244
  timerbase3->TC_RC = 62500;	//__ (25000000 Hz / Verlangsamer ) / 50 Hz = gesamte Periodendauer. 488 Größere Werte aus irgendeinem Grund besser.

  // Start the timer :
  timerbase3->TC_CCR = TC_CLKEN ;				//__Einschalten der clk
  timerbase3->TC_CCR = TC_SWTRG ;				//__Einschalten des counters
  piobaseA->PIO_PER  = (1<<PIOTIOA3) ;	//__Der Port wird Enabled
  piobaseA->PIO_OER  = (1<<PIOTIOA3) ;	//__Der Port wird als Output gesetzt
  piobaseA->PIO_CODR = (1<<PIOTIOA3) ;	//__Output wird gecleart
}

void pumpe(void)
{

	StructPMC* pmcbase	= PMC_BASE;			// Basisadresse des PMC
	StructPIO* piobaseA   	= PIOA_BASE;		// Basisadresse PIO A
	StructPIO* piobaseB   	= PIOB_BASE;		// Basisadresse PIO B
	

	pmcbase->PMC_PCER	= 0x4200;	// enable Periphal clocks for PIOB (0x4000) and TC3 (Timer3, 0x0200)
	
	
// ab hier entsprechend der Aufgabestellung ergänzen
//**************************************************
    StructAIC* aicbase = AIC_BASE; // Initializing the Interrupt thingy
	aicbase->AIC_IDCR = 1<< 14;
	aicbase->AIC_ICCR = 1<<14;
	aicbase->AIC_SVR[PIOB_ID] = (unsigned int) taste_irq_handler;
	aicbase->AIC_SMR[PIOB_ID] = 0x7;
	aicbase->AIC_IECR = 1<<14;
	piobaseB->PIO_IER = KEY1 | KEY2;
	piobaseB->PIO_PER = KEY1 | KEY2;
//	Timer3_init();


	int gewichttmp = 0;
	while((piobaseB->PIO_PDSR & KEY3) || (fuellgewicht <= Maxgewicht))
	{
            //endlos Schleife für das Pumpen
	    gewichttmp = fuellgewicht;
//	    fuellgewicht = (messer() - bechergewicht); 
	    

	}
	
	piobaseA->PIO_PER  = (1<<PIOTIOA3);	//__output port for timer 3 disabled
	aicbase->AIC_IDCR = 1<<14;
	aicbase->AIC_ICCR = 1<<14; 
}



	
int main(void) // OLD MAIN
{
	/*char i;
// Serielle initialisieren  
	inits();
	init_ser();
// CR und LF auf das Terminal ausgeben
	//putc (0xd);
	//putc (0xa);
// ein Zeichen von der seriellen abholen	
	i=getc();
	putc(i);
// String ausgeben
	char a = "hallo welt";
	puts("Hallo! \n");
	puts(a);
	*/
	Timer3_init();
	pumpe();
	return 0;
}
