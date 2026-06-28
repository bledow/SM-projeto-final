#include <msp430.h>

/*
* P1.4 --> A4 recebe a saída do LM358

* Timer 0 gerando PWM de 5 Hz (200ms)
* Então, para janela de 10s --> 50 médias

* SMCLK = 1MHz
* Rc = 50% (gatilho para o ADC)

* calc a média na main()
*/

unsigned int adc_buffer[32];
unsigned char adc_pronto = 0;
unsigned int janela[50];
unsigned char indice_janela = 0;
unsigned char janela_cheia = 0;

int main(void)
{
    volatile unsigned int i;
    WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
    P1DIR |= 0x01;                            // Set P1.0 to output direction

    while(1)
    {
        P1OUT ^= 0x01;                        // Toggle P1.0 using exclusive-OR

        for (i=10000; i>0; i--);
  }
}

//uCon

void ini_Timer0(void){
    TA0CTL = TASSEL1 + ID0 + ID1 + MC0;
    TA0CCTL1 = OUTMOD0 + OUTMOD1 + OUTMOD2;
    TA0CCR0 = 24999; // 1M / 8 * 0,2 - 1
    TA0CCR1 = 12500;
}

void ini_ADC(void){
    ADC10CTL0 = ADC10SHT0 + MSC + ADC10IE + ADC10ON; //freq tipica do LM358 300ohm
    ADC10CTL1 = INCH2 + SHS0 + ADC10SSEL0 + ADC10SSEL1 + CONSEQ1;
    
    ADC10DTC1 = 32;
    ADC10SA = &adc_buffer[0];

    ADC10AE0 = BIT4;
    ADC10CTL0 |= ENC;
}
