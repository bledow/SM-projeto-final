#include <msp430.h>
#include <stdio.h>

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
void enviar_tensao_uart(void);

void analisa_apneia(void);

volatile unsigned int adc_buffer[32];

volatile unsigned int janela[50];
volatile unsigned char indice_janela = 0;
volatile unsigned char janela_cheia = 0;

volatile unsigned long soma = 0;
volatile unsigned int media = 0;

volatile unsigned char janelas_sem_movimento = 0;
volatile unsigned char alarme_ativo = 0;

volatile unsigned long soma_janela = 0;
volatile unsigned int media_janela = 0;
volatile unsigned long variancia_janela = 0;
volatile unsigned long diferenca = 0;

volatile unsigned char log0 = 0;
volatile unsigned char teste4_pronto = 0;

volatile float tensao = 0.0;
unsigned char TX_DATA[32];
unsigned char tx_index = 0;

int main(void)
{
    ini_uCon();
    ini_P1_P2();
    ini_Timer0();
    ini_Timer1();
    ini_ADC();
    ini_uart();

    while(1);
}

void ini_uCon(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    DCOCTL = CALDCO_8MHZ;
    BCSCTL1 = CALBC1_8MHZ;
    BCSCTL2 = DIVS0 + DIVS1;   // SMCLK = 8 MHz / 8 = 1 MHz
    BCSCTL3 = XCAP0 + XCAP1;

    while(BCSCTL3 & LFXT1OF);

    __enable_interrupt();
}

void ini_P1_P2(void)
{
    /*
     * P1.0 --> LED verde
     * P1.6 --> LED vermelho
     * P1.3 --> S2
     * P1.4 --> A4
     */

    P1DIR = BIT0 + BIT6;
    P1REN = BIT3;
    P1OUT = BIT3 + BIT0;

    P1IES = BIT3;
    P1IFG = 0;
    P1IE = BIT3;

    P2DIR = BIT1;
    P2OUT = 0;
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

    UCA0CTL1 |= UCSWRST;

    UCA0CTL0 = 0;
    UCA0CTL1 = UCSSEL1 + UCSWRST;

    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTL = UCBRS0;


    P1SEL  |= BIT2;
    P1SEL2 |= BIT2;

    UCA0CTL1 &= ~UCSWRST;

    IFG2 &= ~UCA0TXIFG;
    IE2  |= UCA0TXIE;
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

    unsigned char k = 0;

    if((~P1IN) & BIT3){
        P1OUT &= ~BIT6;
        P1OUT |= BIT0;
        // P2OUT &= ~BITx; // desligar buzzer

        TA1CTL |= TACLR;

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

    unsigned char k = 0;    
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

    enviar_tensao_uart();
    ADC10SA = (unsigned int) &adc_buffer[0];
    ADC10CTL0 |= ENC;
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_RTI(void){
    IFG2 &= ~UCA0TXIFG;

    if( TX_DATA[tx_index] == '\0' ){
        tx_index = 0;
    }else{
        if(tx_index >= 32){
            tx_index = 0;
        }else{
            UCA0TXBUF = TX_DATA[tx_index];
            tx_index++;
        }
    }
}

/*func apoio*/

void analisa_apneia(void){
    unsigned char k = 0;
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

        if(janelas_sem_movimento >= 2){
            alarme_ativo = 1;

            P1OUT |= BIT6;
            P1OUT &= ~BIT0;
            // P2OUT |= BITx; // ligar buzzer
        }
    } else {
        janelas_sem_movimento = 0;
    }
}

void enviar_tensao_uart(void){

    float aux;
    char vin_int;
    char vin_dec_1;
    char vin_dec_2;

    tensao = (3.3 * (float)media) / 1023.0;

    vin_int = (char)tensao;
    aux = (tensao - (float)vin_int) * 10.0;
    vin_dec_1 = (char)aux;
    aux = (aux - (float)vin_dec_1) * 10.0;
    vin_dec_2 = (char)aux;

    if(tx_index == 0)
    {
        sprintf((char*)TX_DATA,"Tensao = %d,%d%d V\r\n", vin_int, vin_dec_1, vin_dec_2);
        IFG2 |= UCA0TXIFG;
    }
}
