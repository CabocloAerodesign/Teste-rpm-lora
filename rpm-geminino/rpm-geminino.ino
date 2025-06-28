#include <SPI.h>
#include <LoRa.h>

// --- Configurações do LoRa ---
#define LORA_SS_PIN    10 // Chip Select
#define LORA_RST_PIN   9  // Reset
#define LORA_DIO0_PIN  2  // Pino de interrupção para TxDone/RxDone


// --- Configurações do Sensor de RPM ---
#define SENSOR_RPM_PIN 3  // Pino de interrupção para o sensor óptico
#define PULSOS_POR_REVOLUCAO 4.0 // Mude para o número de furos/ranhuras do seu disco

// --- Configurações de Temporização ---
const long INTERVALO_TX = 500; // Intervalo entre envios em milissegundos (2 segundos)
unsigned long tempoAnterior = 0;

// --- Variáveis Globais Voláteis ---
// Usadas em interrupções, devem ser 'volatile'
volatile unsigned long contadorDePulsos = 0;
volatile bool transmissaoConcluida = true; // Começa como true para permitir o primeiro envio

// Função de callback para a interrupção do LoRa (quando a transmissão termina)
void onTxDone();

// Função ISR para a interrupção do sensor óptico (a cada pulso)
void contarPulso();


void setup() {
  Serial.begin(9600);
  while (!Serial);
  Serial.println("Transmissor LoRa com Medidor de RPM por Interrupção");

  // --- Configuração do Sensor de RPM ---
  pinMode(SENSOR_RPM_PIN, INPUT_PULLUP); // Usa o resistor de pull-up interno
  // Anexa a interrupção ao pino do sensor, acionada na borda de DESCIDA (FALLING)
  attachInterrupt(digitalPinToInterrupt(SENSOR_RPM_PIN), contarPulso, FALLING);

  // --- Configuração do LoRa ---
  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(433E6)) { // Frequência 915MHz para o Brasil
    Serial.println("Falha ao iniciar o LoRa!");
    while (1);
  }
  
  // Registra a função de callback para o evento de transmissão concluída
  LoRa.onTxDone(onTxDone);
  
  Serial.println("Setup concluído. Medindo RPM...");
}

void loop() {
  // Verifica se o intervalo de tempo para envio foi atingido
  if (millis() - tempoAnterior >= INTERVALO_TX) {
    
    // Só prossegue se a transmissão anterior já tiver sido concluída
    if (transmissaoConcluida) {
      // --- Seção Crítica: Leitura e Reset do contador ---
      // Desativa interrupções temporariamente para ler e zerar o contador com segurança
      noInterrupts();
      unsigned long pulsos = contadorDePulsos;
      contadorDePulsos = 0;
      interrupts(); // Reativa as interrupções o mais rápido possível

      // --- Cálculo do RPM ---
      // Frequência em Hz = pulsos / intervalo em segundos
      float frequencia = (float)pulsos / (INTERVALO_TX / 1000.0);
      // RPM = (frequência / pulsos por revolução) * 60
      float rpm = (frequencia / PULSOS_POR_REVOLUCAO) * 60.0;
      
      Serial.print("Calculando... Pulsos: ");
      Serial.print(pulsos);
      Serial.print(", RPM: ");
      Serial.println(rpm);

      // --- Transmissão LoRa Assíncrona ---
      transmissaoConcluida = false; // Sinaliza que uma transmissão está prestes a começar
      
      LoRa.beginPacket();
      LoRa.print("RPM:");
      LoRa.print(rpm);
      LoRa.endPacket(true); // O 'true' torna o envio ASSÍNCRONO

      Serial.println("Iniciando transmissão LoRa...");
    } else {
      Serial.println("Aguardando transmissão anterior ser concluída...");
    }
    
    tempoAnterior = millis(); // Atualiza o tempo para o próximo ciclo
  }
  
  // O Arduino pode fazer outras tarefas aqui ou entrar em modo de baixo consumo
}


// ISR da Interrupção do LoRa: Chamada quando a transmissão é finalizada.
// Mantenha esta função o mais simples e rápida possível!
void onTxDone() {
  Serial.println("Transmissão LoRa concluída (TxDone)!");
  transmissaoConcluida = true;
}

// ISR da Interrupção do Sensor: Chamada a cada pulso do sensor.
// Mantenha esta função o mais simples e rápida possível!
void contarPulso() {
  contadorDePulsos++;
}