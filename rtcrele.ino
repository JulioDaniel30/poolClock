#include <RtcDS1302.h>

// Pinos
const int RelePin = 13;
const int btPIn = 12;  // Interruptor
const int ledBLig = 10;

// Estado
bool lig = false;
bool blig = false;
bool ignorarInterruptor = false;
bool estadoAnteriorInterruptor = HIGH;

// Horários
int hlig = 7;
int hdes = 17;

// RTC
ThreeWire myWire(4, 5, 3); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Serial.begin(57600);
  pinMode(RelePin, OUTPUT);
  pinMode(ledBLig, OUTPUT);
  pinMode(btPIn, INPUT_PULLUP);  // Interruptor com GND

  digitalWrite(RelePin, LOW);
  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) Rtc.SetDateTime(compiled);
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
  if (Rtc.GetDateTime() < compiled) Rtc.SetDateTime(compiled);

  Serial.println("Sistema iniciado");
}

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  if (!now.IsValid()) {
    Serial.println("Erro no RTC.");
    return;
  }

  // Log da hora
  Serial.print("Hora atual: ");
  printDateTime(now);
  Serial.println();

  bool estadoAtualInterruptor = digitalRead(btPIn);

  // Se o horário for crítico, desativa manual e bloqueia reativação
  if ((now.Hour() == 17 || now.Hour() == 22 || now.Hour() == 0)) {
    if (blig) {
      blig = false;
      ignorarInterruptor = true;
      Serial.println("blig desligado automaticamente por horário crítico.");
    }
  }

  // Se o interruptor for desligado, remove o bloqueio
  if (estadoAtualInterruptor == HIGH && estadoAnteriorInterruptor == LOW) {
    ignorarInterruptor = false;
    Serial.println("Interruptor foi desligado. Resetando bloqueio.");
  }

  // Se interruptor está ligado e não está bloqueado, ativa blig
  if (estadoAtualInterruptor == LOW && !ignorarInterruptor) {
    if (!blig) {
      blig = true;
      Serial.println("blig ativado manualmente.");
    }
  } else if (estadoAtualInterruptor == HIGH) {
    if (blig) {
      blig = false;
      Serial.println("blig desativado manualmente.");
    }
  }

  estadoAnteriorInterruptor = estadoAtualInterruptor;

  // LED indicativo
  digitalWrite(ledBLig, blig ? HIGH : LOW);

  // Lógica automática
  lig = (now.Hour() >= hlig && now.Hour() < hdes);

  // Controle do relé
  bool estadoRele = (lig || blig);
  digitalWrite(RelePin, estadoRele ? HIGH : LOW);

  Serial.print("Relé: ");
  Serial.println(estadoRele ? "LIGADO" : "DESLIGADO");
  Serial.println("----------------------------");

  delay(1000);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime& dt) {
  char datestring[26];
  snprintf_P(datestring, countof(datestring), PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(), dt.Day(), dt.Year(),
             dt.Hour(), dt.Minute(), dt.Second());
  Serial.print(datestring);
}
