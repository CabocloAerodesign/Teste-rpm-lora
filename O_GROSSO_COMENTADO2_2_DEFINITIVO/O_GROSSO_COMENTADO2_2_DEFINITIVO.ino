//variaveis globais
// INCLUSÃO DE BIBLIOTECAS
#include <HX711.h>
#include <SPI.h>
#include <LoRa.h>
#define SENSOR_RPM_PIN 3
#define pinDT  A5
#define pinSCK  A4
#define SBIT(N) (1<<N)

// INSTANCIANDO OBJETOS
HX711 scale;

// DECLARAÇÃO DE VARIÁVEIS
float medida = 0;
volatile unsigned int sn = 0;// Armazenar valor de n
volatile byte bc =0; //Sinalização para contar pulsos
volatile unsigned int n = 0; //contagem de pulsos
volatile byte ac =0; // Sinalizado para calculo e impresão de RPM
volatile int t=0;// contar tempo
volatile int RPM=0;
volatile unsigned int tempo=0;
volatile int contagem=0;
volatile char ch;//Comunicação serial

void contarPulso() {
  bc=1;
}

//configuração de timer 1 de 200ms 
void confT1(){
  //prrscaler 256x cs1[2...0]=100
  //WGM1[3...0]=0100
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B |= SBIT(WGM12) | SBIT(CS12);
  OCR1A = 12499; // para gerar evento de 200ms
  TCNT1 = 0;
  TIMSK1 |= SBIT(OCIE1A); //Habilitando interrupções
}
  //configuração de timer 2 de 100us 
void confT2(){
  //  prescaler 8x cs2[2...0]=010
  //WGM2[2...0]=010
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2A |= SBIT(WGM21);
  TCCR2B |= SBIT(CS21);
  OCR2A = 199; // para gerar evento de 100 us
  TCNT2 = 0;
  TIMSK2 |= SBIT(OCIE2A); //Habilitando interrupções
}
void serialEvent(){
  while(Serial.available()){
    ch=Serial.read();
  }
}

//maquina de estados para tratamento de debouce
enum estado_t {I,F} estado=I;
void maquinadebou(){
  switch(estado){
    case I://estado para contar n
    if(bc){//só conta se bc =1
      n++;//acressenta n em 1
      estado=F;
      }
      break;

    case F:// zera bc
   bc=0;
   estado = I;
    break;
    default:
    estado= I;
    }
  }
 //maquina de estados para calculo de RPM e tempo
 enum state_t {R, T} state=R;
 void maquinaUart(){
  switch(state){
    case R:// estado para calculo do RPM
    RPM = sn*75;//calculo de RPM
    if(ac){
      // Serial.println(RPM);//impressão de RPM a cada 200ms
       ac=0;
    }
    state=T;
    break;

    case T:// estado para calculo do tempo
    if(n%2==0){
      tempo=t*100;//calculo de tempo em us
    if(ch=='t'){
        Serial.println(tempo);//impressão de do tempo quando 't' for escrito no monitor serial
    }
    t=0;//zera t
    } 
    state=R;
    break;

    default:
    state=R;
  }
 }


//Interrupção timer 1
ISR(TIMER1_COMPA_vect){
  sn = n;//sn armazena valor de n
  n=0;//zerar n
  ac=1;//Sinalizando a Impressão
  contagem++;
  }
//Interrupção timer 2
ISR(TIMER2_COMPA_vect){
  t++;//Contar t
  maquinadebou();//faz com que o n só conte uma vez a cada 100us
  maquinaUart();//Faz calculo e imprime o RPM 
}
//Interrupção externa do sensor que acionada a cada borda de descida





void setup() {
  // put your setup code here, to run once:
//confpin();
confT1();
confT2();
Serial.begin(9600);
 while (!Serial);

  Serial.println("LoRa Sender");

if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
}
serialEvent();
pinMode(SENSOR_RPM_PIN, INPUT_PULLUP);
attachInterrupt(digitalPinToInterrupt(SENSOR_RPM_PIN), contarPulso, FALLING);

scale.begin(pinDT, pinSCK); // CONFIGURANDO OS PINOS DA BALANÇA
scale.set_scale(-43850); // LIMPANDO O VALOR DA ESCALA
  //scale.set_scale(-14616);
scale.power_up(); //ligando sensor;
scale.tare(); // ZERANDO A BALANÇA PARA DESCONSIDERAR A MASSA DA ESTRUTURA
Serial.println("Balança iniciada");
}

void loop() {
  Serial.println("Sending packet: ");
  Serial.println(" ");
  
  medida = scale.get_units(5); // SALVANDO NA VARIAVEL O VALOR DA MÉDIA DE 5 MEDIDAS

  Serial.print("Kilos: ");
  Serial.println(medida, 3); // ENVIANDO PARA MONITOR SERIAL A MEDIDA COM 3 CASAS DECIMAIS
  Serial.print("Newtons: ");
  Serial.println(medida*9.81, 3);
  Serial.print("RPM: ");
  Serial.println(RPM);
  Serial.println(" ");


  // send packet
  LoRa.beginPacket();
  LoRa.print("RPM: ");
  LoRa.println(RPM);
  LoRa.print("kilos: ");
  LoRa.println(medida, 3);
  LoRa.print("Newtons: ");
  LoRa.println(medida*9.81, 3);
  LoRa.endPacket();
  delay(3000);
}
