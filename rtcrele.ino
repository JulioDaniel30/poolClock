#include <RtcDS1302.h>

// === Definições de pinos ===
const int RelePin = 13;
const int btPIn = 12;         // Interruptor para ligar manualmente (blig)
const int ledBLig = 10;

// === Variáveis de controle ===
bool lig = false;
bool blig = false;

int hlig = 7;
int hdes = 17;

// Controle de borda e reset
bool btPrev = HIGH;       // Estado anterior do interruptor manual
bool bligResetado = false;

ThreeWire myWire(4, 5, 3); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
    Serial.begin(57600);

    pinMode(RelePin, OUTPUT);
    pinMode(ledBLig, OUTPUT);
    pinMode(btPIn, INPUT_PULLUP);
    digitalWrite(RelePin, LOW);

    Rtc.Begin();
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);

    if (!Rtc.IsDateTimeValid()) {
        Serial.println("RTC inválido! Atualizando.");
        Rtc.SetDateTime(compiled);
    }

    if (Rtc.GetIsWriteProtected()) {
        Rtc.SetIsWriteProtected(false);
    }

    if (!Rtc.GetIsRunning()) {
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) {
        Rtc.SetDateTime(compiled);
    }
}

void loop() {
    RtcDateTime now = Rtc.GetDateTime();
    if (!now.IsValid()) {
        Serial.println("Erro no RTC.");
        return;
    }

    printDateTime(now);
    Serial.println();

    // === Leitura do interruptor manual ===
    bool btAtual = digitalRead(btPIn);

    // === Reset automático de blig às 17h, 22h ou 00h ===
    if ((now.Hour() == 17 || now.Hour() == 22 || now.Hour() == 0) && !bligResetado) {
        blig = false;
        bligResetado = true;
        Serial.println("blig resetado por horário");
    }

    // === Detecta borda de subida (ligar chave após reset) ===
    if (btAtual == LOW && btPrev == HIGH && bligResetado) {
        blig = true;
        bligResetado = false;
        Serial.println("blig reativado pelo usuário");
    }

    btPrev = btAtual; // Atualiza estado anterior da chave

    // === LED de indicação ===
    digitalWrite(ledBLig, blig ? HIGH : LOW);

    // === Controle automático por horário ===
    lig = (now.Hour() >= hlig && now.Hour() < hdes);

    // === Controle do relé ===
    digitalWrite(RelePin, (lig || blig) ? HIGH : LOW);

    // === Libera novo reset após sair dos horários críticos ===
    if (now.Hour() != 0 && now.Hour() != 17 && now.Hour() != 22) {
        bligResetado = false;
    }

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
