#include <msp430.h>

/*
* P1.4 --> A4 recebe a saída do LM358

* Timer 0 gerando PWM de 5 Hz (200ms)
* Então, para janela de 10s --> 50 médias

* SMCLK = 1MHz
* Rc = 50% (gatilho para o ADC)

*  cálculo da média na RTI do ADC

* 1 un. de ADC = 3,3V / 1023 = 3,23mV
* 50mV / 3,23mV = 15,5 => limiar = 16
*/

void ini_uCon(void);
void ini_P1_P2(void);
void ini_Timer0(void);
void ini_Timer1(void);
void ini_ADC(void);
void ini_uart(void);

void analisa_apneia(void);

unsigned int adc_buffer[32];

unsigned int janela[50];
unsigned char indice_janela = 0;
unsigned char janela_cheia = 0;

unsigned long soma = 0;
unsigned int media = 0;

unsigned char janelas_sem_movimento = 0;
unsigned char alarme_ativo = 0;

unsigned long soma_janela = 0;
unsigned int media_janela = 0;
unsigned long variancia_janela = 0;
long diferenca = 0;

unsigned char k = 0;

int main(void)
{
    ini_uCon();
    ini_P1_P2();
    ini_Timer0();
    ini_Timer1();
    ini_ADC();
    ini_uart();

    __enable_interrupt();

    alarme_ativo = 1;

    P1OUT |= BIT0;     // vermelho
    P1OUT &= ~BIT6;    // verde apagado
    //P2OUT |= BIT1;     // buzzer ou saída de teste

    while(1);
}



void ini_uCon(void){
    WDTCTL = WDTPW + WDTHOLD;

    //ini
    if(CALBC1_1MHZ == 0xFF) while(1);

    DCOCTL = 0;
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
}

void ini_P1_P2(void){
    //ini
    P1DIR |= BIT0 + BIT6;
    P1OUT &= ~BIT0;
    P1OUT |= BIT6;

    P1DIR &= ~BIT3;
    P1REN |= BIT3;
    P1OUT |= BIT3;
    P1IES |= BIT3;
    P1IFG &= ~BIT3;
    P1IE |= BIT3;

    P2DIR |= BIT1;
    P2OUT &= ~BIT1;
}

void ini_Timer0(void){
    TA0CTL = TASSEL1 + ID0 + ID1 + MC0;
    TA0CCTL1 = OUTMOD0 + OUTMOD1 + OUTMOD2;
    TA0CCR0 = 24999; // 1M / 8 * 0,2 - 1
    TA0CCR1 = 12500;
}

void ini_Timer1(void){
    //ini
    TA1CTL = TASSEL1 + TACLR;
    TA1CCR0 = 14999;
    TA1CCTL0 = CCIE;
}

void ini_ADC(void){
    ADC10CTL0 = ADC10SHT0 + MSC + ADC10IE + ADC10ON; //freq tipica do LM358 300ohm
    ADC10CTL1 = INCH2 + SHS0 + ADC10SSEL0 + ADC10SSEL1 + CONSEQ1;
    
    ADC10DTC1 = 32;
    ADC10SA = (unsigned int) &adc_buffer[0];

    ADC10AE0 = BIT4;
    ADC10CTL0 |= ENC;
}

void ini_uart(void){
    //ini
}

/* RTI */

#pragma  vector=PORT1_VECTOR
__interrupt void RTI_porta1(void){
    P1IE &= ~BIT3;
    TA1CTL |= MC0;
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void RTI_M0_Timer1_deb(void){
    TA1CTL &= ~MC0;

    if((~P1IN) & BIT3){
        P1OUT &= ~BIT0;
        P1OUT |= BIT6;
        // P2OUT &= ~BITx; // desligar buzzer

        alarme_ativo = 0;
        janelas_sem_movimento = 0;
        variancia_janela = 0;
        media_janela = 0;
        soma_janela = 0;

        indice_janela = 0;
        janela_cheia = 0;

        for(k = 0; k < 50; k++){
            janela[k] = 0;
        }
    }

    P1IFG &= ~BIT3;
    P1IE |= BIT3;
}

#pragma vector=ADC10_VECTOR
__interrupt void RTI_ADC(void){
    ADC10CTL0 &= ~ENC;
    
    soma = 0;

    for(k = 0; k < 32; k++) soma = soma + adc_buffer[k];

    media = soma >> 5;

    janela[indice_janela] = media;

    if(indice_janela >= 49){
        janela_cheia = 1;
        indice_janela = 0;
    } else {
        indice_janela++;
    }

    if(janela_cheia){
        analisa_apneia();
        janela_cheia = 0;
    }

    ADC10SA = (unsigned int) &adc_buffer[0];
    ADC10CTL0 |= ENC;
}

/*func apoio*/

void analisa_apneia(void){
    soma_janela = 0;

    for(k = 0; k < 50; k++) soma_janela += janela[k];

    media_janela = soma_janela / 50;

    variancia_janela = 0;

    for(k = 0; k < 50; k++){
        diferenca = (long) janela[k] - media_janela;
        variancia_janela = variancia_janela + diferenca * diferenca;
    }

    variancia_janela = variancia_janela / 50;

    if(alarme_ativo) return;

    if(variancia_janela < 256){
        janelas_sem_movimento++;
        if( janelas_sem_movimento >= 2){
            alarme_ativo = 1;

            P1OUT |= BIT0;
            P1OUT &= ~BIT6;
            // P2OUT |= BITx; // ligar buzzer
        }
    } else {
        janelas_sem_movimento = 0;
    }
}
