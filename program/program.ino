#include <util/delay.h>
//using pins d2,d3,d4
#define PPORT PORTD
#define PDDR DDRD
#define PPIN PIND
#define PCLK 2
#define PDAT 3
#define PMCLR 4

/*
this implementation is specific to the PIC10F202, not tried on other pics
*/

#define FLASH_SIZE 512//in words
unsigned short flash[FLASH_SIZE];//each byte grouping is big endian

void pclk1(void){PPORT|=(1<<PCLK);}
void pclk0(void){PPORT&=~(1<<PCLK);}
void pclkd1(void){PDDR|=(1<<PCLK);}
void pclkd0(void){PDDR&=~(1<<PCLK);}

void pdat1(void){PPORT|=(1<<PDAT);}
void pdat0(void){PPORT&=~(1<<PDAT);}
void pdatd1(void){PDDR|=(1<<PDAT);}
void pdatd0(void){PDDR&=~(1<<PDAT);}
unsigned char pdatb(void){return (PPIN&(1<<PDAT))&&1;}

void pmclr1(void){PPORT|=(1<<PMCLR);}
void pmclr0(void){PPORT&=~(1<<PMCLR);}
void pmclrd1(void){PDDR|=(1<<PMCLR);}
void pmclrd0(void){PDDR&=~(1<<PMCLR);}

void pcycle(void){
  pclk1();
  _delay_us(10);
  pclk0();
  _delay_us(10); 
}

void exit_prog_mode(void){
  pclk0();
  pdat0();
  _delay_us(10);
  pmclr1();
  _delay_ms(10);
}

void enter_prog_mode(void){
  pmclr0();
  _delay_us(500);
}

void usart_tx(unsigned char d){
//wait for buffer to be empty
  while (!(UCSR0A&(1<<UDRE0)));
  UDR0=d;
}

void usart_rx(unsigned char *d){
//wait for unread buffer data
  while (!(UCSR0A&(1<<RXC0)));
  *d=UDR0;
}

void psend(unsigned short d,unsigned char c){     
  pdatd1();
  for (unsigned char i=0;i<c;i++){
    pclk1();
    switch (d&0x01){
      case 1: pdat1(); break;
      case 0: pdat0(); break;
    }
    d=d>>1;
    _delay_us(10);
    pclk0();
    _delay_us(10);
  }
  pdatd0();
  pdat0();  
}

void pclear(){
  enter_prog_mode();
  psend(0x09,6);
  _delay_ms(10);
  exit_prog_mode();
}

unsigned short pread(void){
  unsigned short buf=0x0000;
//disable pullup and make data pin an input
  pdat0();
  pdatd0();
//ignore start bit
  pcycle();
  for (int i=0;i<14;i++){
    pclk1();
    _delay_us(10);
    buf|=pdatb()<<i;
    pclk0();
    _delay_us(10);
  }
//ignore stop bit
  pcycle();
  pdat0();
  pdatd1();
  return buf;
}

int main(void){
//2.4k baud
  UBRR0L=416;
  UBRR0H=416>>8;
//enable tx and rx
  UCSR0B|=(1<<RXEN0)|(1<<TXEN0);
//define pin states and pin directions
  pmclr1();
  pclk0();
  pdat0(); 
  pclkd1();
  pdatd1();
  pmclrd1();
  unsigned char buf;
  unsigned short adr;
  while (1){
    usart_rx(&buf);
    switch(buf){
      case 0x01:
        usart_tx(0x02);
        break;
      case 0x02:
        {
          enter_prog_mode();
          for (int i=0;i<FLASH_SIZE;i++){
            psend(0x06,6);
            psend(0x04,6);
            flash[i]=pread();
          }
          exit_prog_mode();
          usart_tx(0x01);
        }        
        break;
      case 0x03:
        {
          usart_rx(&buf);
          adr=buf<<8;
          usart_rx(&buf);        
          adr|=buf;
          usart_rx(&buf);
          adr=adr>>1;
          unsigned short *ptr=flash+adr;
          unsigned char len=buf;
          for (int i=0;i<len;i++){
            usart_rx(&buf);
            *ptr=buf;
            usart_rx(&buf);
            *(ptr++)|=buf<<8;                      
          }
          usart_tx(0x01);
        }
        break;
      case 0x04:
        {
          pclear();
          enter_prog_mode();
          for (int i=0;i<FLASH_SIZE;i++){
            psend(0x06,6);
            if (flash[i]==0xffff)
              continue;
            psend(0x02,6);
            psend(0xff,1);//start bit      
            psend(flash[i],12);//data
            psend(0xff,3);//stop plus 2 msb ignored
            psend(0x08,6);
            _delay_ms(2);
            psend(0x0e,6);
            _delay_us(200);
          }
          exit_prog_mode();
          usart_tx(0x01);
        }
        break;
      case 0x05:
        memset(flash,0xff,sizeof(flash));
        usart_tx(0x01);        
        break;
      case 0x06:
        {
          usart_rx(&buf);
          adr=buf<<8;
          usart_rx(&buf);
          adr|=buf;
          unsigned short *ptr=flash+adr;
//todo check address is in range                 
          usart_tx(*ptr>>8);
          usart_tx(*ptr);
        }
        break;
    }
  }
  return 0;
}