#include <RtcDS1302.h>

// --- PINOS ---
const int pinoRele = 13;
const int pinoInterruptor = 12; // Interruptor manual
const int pinoLedManual = 10;

// --- ESTADOS ---
bool horarioAutomaticoAtivo = false;
bool acionamentoManualAtivo = false;
bool ignorarInterruptor = false;

// --- CONTROLE DO INTERRUPTOR (DEBOUNCE) ---
bool estadoAtualInterruptor = HIGH;
bool ultimoEstadoInterruptor = HIGH;
unsigned long ultimaVerificacaoDebounce = 0;
const long delayDebounce = 50; // 50 milissegundos

// --- HORÁRIOS ---
const int horaLigar = 7;
const int horaDesligar = 17;
const int horasCriticas[] = {17, 22, 0}; // Horas para desligamento forçado

// --- RTC ---
ThreeWire myWire(4, 5, 3); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Serial.begin(57600);
  pinMode(pinoRele, OUTPUT);
  pinMode(pinoLedManual, OUTPUT);
  pinMode(pinoInterruptor, INPUT_PULLUP); // Interruptor com GND

  digitalWrite(pinoRele, LOW);
  Rtc.Begin();

  // Configuração inicial do RTC (só na primeira vez ou se a bateria falhar)
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC não configurado! Configurando com a hora da compilação.");
    Rtc.SetDateTime(compiled);
  }
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);

  Serial.println("Sistema iniciado.");
}

void loop() {
  RtcDateTime agora = Rtc.GetDateTime();
  if (!agora.IsValid()) {
    Serial.println("Erro na leitura do RTC. Verifique a bateria e as conexões.");
    delay(5000); // Espera antes de tentar novamente para não poluir o Serial
    return;
  }

  // --- LÓGICA DO INTERRUPTOR MANUAL COM DEBOUNCE ---
  bool leituraInterruptor = digitalRead(pinoInterruptor);

  // Verifica se o estado do interruptor mudou
  if (leituraInterruptor != ultimoEstadoInterruptor) {
    ultimaVerificacaoDebounce = millis();
  }

  // Após um tempo de debounce, confirma a mudança de estado
  if ((millis() - ultimaVerificacaoDebounce) > delayDebounce) {
    // Se o estado realmente mudou, atualiza
    if (leituraInterruptor != estadoAtualInterruptor) {
      estadoAtualInterruptor = leituraInterruptor;

      // Se o interruptor foi LIGADO (LOW) e não está bloqueado
      if (estadoAtualInterruptor == LOW && !ignorarInterruptor) {
        if (!acionamentoManualAtivo) {
          acionamentoManualAtivo = true;
          Serial.println("Acionamento manual ATIVADO.");
        }
      } 
      // Se o interruptor foi DESLIGADO (HIGH)
      else if (estadoAtualInterruptor == HIGH) {
        if (acionamentoManualAtivo) {
          acionamentoManualAtivo = false;
          Serial.println("Acionamento manual DESATIVADO.");
        }
        // Ao desligar o interruptor, remove qualquer bloqueio existente
        if (ignorarInterruptor) {
          ignorarInterruptor = false;
          Serial.println("Bloqueio do interruptor removido.");
        }
      }
    }
  }
  ultimoEstadoInterruptor = leituraInterruptor;

  // --- LÓGICA DE HORÁRIO CRÍTICO ---
  bool ehHoraCritica = false;
  for (int hora : horasCriticas) {
    // Verifica se a hora é crítica E se estamos no primeiro minuto dessa hora
    if (agora.Hour() == hora && agora.Minute() == 0) {
      ehHoraCritica = true;
      break;
    }
  }
  
  if (ehHoraCritica) {
    if (acionamentoManualAtivo) {
      acionamentoManualAtivo = false;
      ignorarInterruptor = true; // Bloqueia até que o interruptor seja desligado
      Serial.print("HORA CRÍTICA (");
      Serial.print(agora.Hour());
      Serial.println("h): Acionamento manual desligado e bloqueado.");
    }
  }

  // --- LÓGICA DE HORÁRIO AUTOMÁTICO ---
  horarioAutomaticoAtivo = (agora.Hour() >= horaLigar && agora.Hour() < horaDesligar);

  // --- CONTROLE FINAL DO RELÉ ---
  bool ligarRele = (horarioAutomaticoAtivo || acionamentoManualAtivo);
  digitalWrite(pinoRele, ligarRele ? HIGH : LOW);
  
  // --- ATUALIZAÇÃO DOS INDICADORES ---
  digitalWrite(pinoLedManual, acionamentoManualAtivo);
  
  // --- LOG NO SERIAL ---
  Serial.print(agora.Hour(), DEC); Serial.print(":"); Serial.print(agora.Minute(), DEC); Serial.print(":"); Serial.println(agora.Second(), DEC);
  Serial.print("  - Auto: "); Serial.print(horarioAutomaticoAtivo ? "LIGADO" : "DESLIGADO");
  Serial.print(" | Manual: "); Serial.print(acionamentoManualAtivo ? "LIGADO" : "DESLIGADO");
  Serial.print(" | Rele: "); Serial.println(ligarRele ? "LIGADO" : "DESLIGADO");
  Serial.println("------------------------------------");

  delay(1000);
}
