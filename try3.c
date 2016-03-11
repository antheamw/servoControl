//  try2.c
// control motor with PWM

#include <C8051F38x.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <c8051f38x.h>
#include <math.h>

#define SYSCLK    48000000L // SYSCLK frequency in Hz
#define BAUDRATE  115200L   // Baud rate of UART in bps

// ANSI colors
#define	COLOR_BLACK		0
#define	COLOR_RED		1
#define	COLOR_GREEN		2
#define	COLOR_YELLOW	3
#define	COLOR_BLUE		4
#define	COLOR_MAGENTA	5
#define	COLOR_CYAN		6
#define	COLOR_WHITE		7

// Some ANSI escape sequences
#define CURSOR_ON "\x1b[?25h"
#define CURSOR_OFF "\x1b[?25l"
#define CLEAR_SCREEN "\x1b[2J"
#define GOTO_YX "\x1B[%d;%dH"
#define CLR_TO_END_LINE "\x1B[K"


#define OUT0 P2_0
#define OUT1 P2_1

volatile unsigned char pwm_count = 0;
volatile int PWM0 = 100;
volatile int PWM1 = 0;
char change = 'n';
char CorA = 'c';

char _c51_external_startup (void)
{
	PCA0MD&=(~0x40) ;    // DISABLE WDT: clear Watchdog Enable bit
	VDM0CN=0x80; // enable VDD monitor
	RSTSRC=0x02|0x04; // Enable reset on missing clock detector and VDD

	// CLKSEL&=0b_1111_1000; // Not needed because CLKSEL==0 after reset
	#if (SYSCLK == 12000000L)
		//CLKSEL|=0b_0000_0000;  // SYSCLK derived from the Internal High-Frequency Oscillator / 4 
	#elif (SYSCLK == 24000000L)
		CLKSEL|=0b_0000_0010; // SYSCLK derived from the Internal High-Frequency Oscillator / 2.
	#elif (SYSCLK == 48000000L)
		CLKSEL|=0b_0000_0011; // SYSCLK derived from the Internal High-Frequency Oscillator / 1.
	#else
		#error SYSCLK must be either 12000000L, 24000000L, or 48000000L
	#endif
	OSCICN |= 0x03; // Configure internal oscillator for its maximum frequency

	// Configure UART0
	SCON0 = 0x10; 
#if (SYSCLK/BAUDRATE/2L/256L < 1)
	TH1 = 0x10000-((SYSCLK/BAUDRATE)/2L);
	CKCON &= ~0x0B;                  // T1M = 1; SCA1:0 = xx
	CKCON |=  0x08;
#elif (SYSCLK/BAUDRATE/2L/256L < 4)
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/4L);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 01                  
	CKCON |=  0x01;
#elif (SYSCLK/BAUDRATE/2L/256L < 12)
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2L/12L);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 00
#else
	TH1 = 0x10000-(SYSCLK/BAUDRATE/2/48);
	CKCON &= ~0x0B; // T1M = 0; SCA1:0 = 10
	CKCON |=  0x02;
#endif
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit autoreload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
	
	// Configure the pins used for square output
	P2MDOUT|=0b_0000_0011;
	P0MDOUT |= 0x10; // Enable UTX as push-pull output
	XBR0     = 0x01; // Enable UART on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0x50; // Enable crossbar and weak pull-ups

	// Initialize timer 2 for periodic interrupts
	TMR2CN=0x00;   // Stop Timer2; Clear TF2;
	CKCON|=0b_0001_0000;
	TMR2RL=(-(SYSCLK/(2*48))/(100L)); // Initialize reload value
	TMR2=0xffff;   // Set to reload immediately
	ET2=0;         // Disable Timer2 interrupts
	TR2=1;         // Start Timer2

	EA=1; // Enable interrupts
	
	return 0;
}


void Timer2_ISR (void) interrupt 5
{
	TF2H = 0; // Clear Timer2 interrupt flag
	
	pwm_count++;
	if(pwm_count>100) pwm_count=0;
	
	if(CorA == 'c'){
		OUT0 = pwm_count > PWM0 ? 0:1;
		OUT1 = !OUT0;
	}
	else {
		OUT0 = !(pwm_count > PWM0 ? 0:1);
		OUT1 = !OUT0;
	}
}

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter
	
	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON:
	CKCON|=0b_0100_0000;
	
	TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow
	
	TMR3CN = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN & 0x80));  // Wait for overflow
		TMR3CN &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

// plays sound out of pin P1_4
void playSound(int period) {
	int number = 500000/(period/2);
	P1_4 = 0;
	while(number>0){
		Timer3us (period/6);
		if (P1_4 == 1)
			P1_4 = 0;
		else
			P1_4 = 1;
		number--;
	}
	P1_4 = 0;
}

// reverses a string 'str' of length 'len'
void reverse(char *str, int len)
{
    int i=0, j=len-1, temp;
    while (i<j)
    {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++; j--;
    }
}

 // Converts a given integer x to string str[].  d is the number
 // of digits required in output. If d is more than the number
 // of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x)
    {
        str[i++] = (x%10) + '0';
        x = x/10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating point number to string.
// Input: n - value to be converted
// Input: res - string to store resulting string in
// Input afterpoint - number of digits after the point
void ftoa(float n, char *res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;

    // Extract floating part
    float fpart = n - (float)ipart;

    // convert integer part to string
    int i = intToStr(ipart, res, 0);

    // check for display option after point
    if (afterpoint != 0)
    {
        res[i] = '.';  // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter is needed
        // to handle cases like 233.007
        fpart = fpart * powf(10, afterpoint);

        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}
// pin = 0 means P2.0
// pin = 1 means P2.1
int getPWM(void) {
	int value;
	
	printf(GOTO_YX , 3,1);
	printf("Clockwise (c) or anticlockwise (a)?");
	scanf("%s", &CorA);
	while (CorA != 'a' && CorA != 'c'){
		printf(GOTO_YX , 3,1);
		printf("Error! Please enter clockwise (c) or anticlockwise (a). ");
		printf(CLR_TO_END_LINE);
		scanf ("%s", &CorA);
	}
	printf(GOTO_YX, 3,1);
	if(CorA == 'a')
		printf("Anticlockwise");
	else
		printf("Clockwise");
	printf(CLR_TO_END_LINE);

	printf(GOTO_YX, 4,1);
	printf("What percentage PWM do you want? ");
	printf(CLR_TO_END_LINE);
	scanf ("%d", &value);
	while (value > 100 || value < 0) {
		printf(GOTO_YX, 4,1);	// go to row 2, column 1
		printf("Error! Please enter a number between 0 and 100. ");
		printf(CLR_TO_END_LINE);
		scanf ("%d", &value);
	}
	printf(GOTO_YX, 4,1);	// go to row 2, column 1
	printf("PWM percentage: %d", value);
	printf(CLR_TO_END_LINE);
	return value;
} 

void main (void) {
	int number1;
	int number2;

	int operation;
	int multiplier;
	int temp;
	char answerstring[8];
	char inputanswer[8];
	int answer;
	printf(CLEAR_SCREEN); // Clear screen using ANSI escape sequence.
	printf(CURSOR_OFF);
	printf("Hello!\n");
	playSound(2);
	
	OUT0 = 0;
	OUT1 = 0;	
	
	while(1) {
		printf(GOTO_YX, 3,1);
		operation=rand();
		multiplier=rand();
		number1=rand();
		number2=rand();
		number1=number1%10;
		number2=number2%10;
		multiplier=multiplier%10;
		operation=operation%4;
	
		if(operation==0){
			// add
			answer=number1+number2;
			printf ("%d + %d =", number1, number2);
			printf(CLR_TO_END_LINE);
		}
		else if (operation==1){
			// subtract
			if (number1<number2) {
				temp=number1;
				number1=number2;
				number2=temp;
			}
			answer=number1-number2;
			printf ("%d - %d =", number1, number2);
			printf(CLR_TO_END_LINE);
		}
		else if(operation==2){		// multiply
			answer=number1*number2;
			printf ("%d * %d =", number1, number2);
			printf(CLR_TO_END_LINE);
		}
		else {
			// divide
			number1=number2*multiplier;
			answer=multiplier;
			printf ("%d / %d =", number1, number2);
			printf(CLR_TO_END_LINE);
		}
	
		scanf("%s",inputanswer);
		
		ftoa(answer, answerstring, 0);
		
		if (strcmp(answerstring, inputanswer)==0) {
			printf("You win!");
			printf(CLR_TO_END_LINE);
			playSound(1516);
			waitms(1);
			playSound(1276);
			waitms(1);
			playSound(758);
			waitms(1);
			playSound(956);
			waitms(1);
			playSound(851);
			waitms(1);
			playSound(638);
			
			ET2=1;
			waitms(2000);
			ET2 = 0;
			CorA = 'a'; // switch direction
			ET2=1;
			waitms(500);
			ET2 = 0;
			CorA = 'c';
			OUT0 = 0;
		 	OUT1 = 0;
		}
		else {
			printf(GOTO_YX, 3,1);
			printf("You are wrong, the answer is %d and you inputed %s", answer, inputanswer);
			printf(CLR_TO_END_LINE);
			
			playSound(1250);
			playSound(1250);
			waitms(1);
			playSound(1500);
			playSound(1500);
			waitms(1);
			/*
			playSound(758);
			waitms(1);
			playSound(956);
			playSound(956);
			*/
			waitms(1000);
		}
	}
}
