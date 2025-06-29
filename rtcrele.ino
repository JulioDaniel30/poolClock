#include <RtcDS1302.h>
#include <EEPROM.h> // Biblioteca para usar a memória interna

// --- ENDEREÇOS NA EEPROM ---
// Usamos endereços para organizar onde cada dado é salvo.
const int ADDR_HORA_LIGAR = 0;
const int ADDR_HORA_DESLIGAR = 4; // Um int usa 4 bytes
const int ADDR_MANUAL_ATIVO = 8;   // Um bool usa 1 byte
const int ADDR_IGNORAR_INT = 10;
const int ADDR_CHECK = 12; // Endereço para verificar se a EEPROM já foi configurada

// --- PINOS ---
const int pinoRele = 13;
const int pinoInterruptor = 12;
const int pinoLedManual = 10;

// --- ESTADOS ---
// Agora, os valores padrão serão carregados da EEPROM ou definidos na primeira execução.
int horaLigar;
int horaDesligar;
bool acionamentoManualAtivo;
bool ignorarInterruptor;

// --- CONTROLE DO INTERRUPTOR (DEBOUNCE) ---
bool estadoAtualInterruptor = HIGH;
bool ultimoEstadoInterruptor = HIGH;
unsigned long ultimaVerificacaoDebounce = 0;
const long delayDebounce = 50;

// --- HORÁRIOS ---
const int horasCriticas[] = {17, 22, 0};

// --- RTC ---
ThreeWire myWire(4, 5, 3);
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Serial.begin(57600);
  pinMode(pinoRele, OUTPUT);
  pinMode(pinoLedManual, OUTPUT);
  pinMode(pinoInterruptor, INPUT_PULLUP);

  digitalWrite(pinoRele, LOW);
  
  carregarConfiguracoesDaEEPROM(); // Carrega os últimos valores salvos

  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC não configurado! Configurando com a hora da compilação.");
    Rtc.SetDateTime(compiled);
  }
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);

  Serial.println("Sistema iniciado com configurações da EEPROM.");
}

void loop() {
  RtcDateTime agora = Rtc.GetDateTime();
  if (!agora.IsValid()) {
    Serial.println("Erro na leitura do RTC. Verifique a bateria e as conexões.");
    delay(5000);
    return;
  }

  gerenciarInterruptorManual();
  verificarHorasCriticas(agora);

  // --- LÓGICA DE HORÁRIO AUTOMÁTICO ---
  bool horarioAutomaticoAtivo = (agora.Hour() >= horaLigar && agora.Hour() < horaDesligar);

  // --- CONTROLE FINAL DO RELÉ ---
  bool ligarRele = (horarioAutomaticoAtivo || acionamentoManualAtivo);
  digitalWrite(pinoRele, ligarRele);
  
  // --- ATUALIZAÇÃO DOS INDICADORES ---
  digitalWrite(pinoLedManual, acionamentoManualAtivo);
  
  // --- LOG NO SERIAL ---
  // (Opcional, pode ser removido para um funcionamento mais limpo)
  imprimirStatus(agora, horarioAutomaticoAtivo, ligarRele);

  delay(1000);
}

// ===================================================================
// FUNÇÕES DE GERENCIAMENTO
// ===================================================================

void gerenciarInterruptorManual() {
  bool leituraInterruptor = digitalRead(pinoInterruptor);
  if (leituraInterruptor != ultimoEstadoInterruptor) {
    ultimaVerificacaoDebounce = millis();
  }

  if ((millis() - ultimaVerificacaoDebounce) > delayDebounce) {
    if (leituraInterruptor != estadoAtualInterruptor) {
      estadoAtualInterruptor = leituraInterruptor;

      bool novoEstadoManual = acionamentoManualAtivo;
      bool novoEstadoBloqueio = ignorarInterruptor;

      if (estadoAtualInterruptor == LOW && !ignorarInterruptor) {
        if (!acionamentoManualAtivo) {
          novoEstadoManual = true;
          Serial.println("Acionamento manual ATIVADO.");
        }
      } else if (estadoAtualInterruptor == HIGH) {
        if (acionamentoManualAtivo) {
          novoEstadoManual = false;
          Serial.println("Acionamento manual DESATIVADO.");
        }
        if (ignorarInterruptor) {
          novoEstadoBloqueio = false;
          Serial.println("Bloqueio do interruptor removido.");
        }
      }
      
      // Salva na EEPROM APENAS se o valor mudou
      if (novoEstadoManual != acionamentoManualAtivo) {
        acionamentoManualAtivo = novoEstadoManual;
        EEPROM.put(ADDR_MANUAL_ATIVO, acionamentoManualAtivo);
      }
      if (novoEstadoBloqueio != ignorarInterruptor) {
        ignorarInterruptor = novoEstadoBloqueio;
        EEPROM.put(ADDR_IGNORAR_INT, ignorarInterruptor);
      }
    }
  }
  ultimoEstadoInterruptor = leituraInterruptor;
}

void verificarHorasCriticas(const RtcDateTime& agora) {
  bool ehHoraCritica = false;
  for (int hora : horasCriticas) {
    if (agora.Hour() == hora && agora.Minute() == 0 && agora.Second() < 5) { // Janela de 5s
      ehHoraCritica = true;
      break;
    }
  }
  
  if (ehHoraCritica && acionamentoManualAtivo) {
      acionamentoManualAtivo = false;
      EEPROM.put(ADDR_MANUAL_ATIVO, acionamentoManualAtivo);
      
      ignorarInterruptor = true;
      EEPROM.put(ADDR_IGNORAR_INT, ignorarInterruptor);
      
      Serial.print("HORA CRÍTICA: Acionamento manual desligado e bloqueado.");
  }
}

// ===================================================================
// FUNÇÕES DA EEPROM
// ===================================================================

void carregarConfiguracoesDaEEPROM() {
  // O valor 123 é um "selo" para sabermos que já gravamos os dados antes.
  // Se o valor lido for diferente, significa que é a primeira vez que o código roda.
  byte checkValue;
  EEPROM.get(ADDR_CHECK, checkValue);

  if (checkValue != 123) {
    Serial.println("Primeira execução! Salvando configurações padrão na EEPROM.");
    
    // Define os valores padrão
    horaLigar = 7;
    horaDesligar = 17;
    acionamentoManualAtivo = false;
    ignorarInterruptor = false;

    // Salva tudo na EEPROM
    EEPROM.put(ADDR_HORA_LIGAR, horaLigar);
    EEPROM.put(ADDR_HORA_DESLIGAR, horaDesligar);
    EEPROM.put(ADDR_MANUAL_ATIVO, acionamentoManualAtivo);
    EEPROM.put(ADDR_IGNORAR_INT, ignorarInterruptor);
    
    // Grava o "selo" para não fazer isso de novo
    EEPROM.put(ADDR_CHECK, (byte)123);
    
  } else {
    Serial.println("Carregando configurações da EEPROM...");
    EEPROM.get(ADDR_HORA_LIGAR, horaLigar);
    EEPROM.get(ADDR_HORA_DESLIGAR, horaDesligar);
    EEPROM.get(ADDR_MANUAL_ATIVO, acionamentoManualAtivo);
    EEPROM.get(ADDR_IGNORAR_INT, ignorarInterruptor);
  }
}

void imprimirStatus(const RtcDateTime& agora, bool autoAtivo, bool releLigado) {
    char datestring[20];
    snprintf_P(datestring, sizeof(datestring), PSTR("%02u:%02u:%02u"), agora.Hour(), agora.Minute(), agora.Second());
    Serial.print(datestring);
    Serial.print(" - Auto: "); Serial.print(autoAtivo ? "ON" : "OFF");
    Serial.print(" | Manual: "); Serial.print(acionamentoManualAtivo ? "ON" : "OFF");
    Serial.print(" | Rele: "); Serial.println(releLigado ? "LIGADO" : "DESLIGADO");
}
