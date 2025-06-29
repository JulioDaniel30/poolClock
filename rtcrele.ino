#include <RtcDS1302.h>
#include <EEPROM.h>

// Pinos
const int RelePin = 13;
const int btPin = 12;
const int ledBLig = 10;

// Estado
bool lig = false;  // Estado do controle automático
bool blig = false; // Estado do controle manual (override)
bool horariosAlterados = false; // Flag para indicar se há alterações não salvas

// Horários automáticos
int hlig1, hdes1, hlig2, hdes2;

// EEPROM - endereços
const int EEPROM_ADDR_HLIG1 = 0;
const int EEPROM_ADDR_HDES1 = 1;
const int EEPROM_ADDR_HLIG2 = 2;
const int EEPROM_ADDR_HDES2 = 3;

// RTC
ThreeWire myWire(4, 5, 3); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

// Debounce para o botão
bool ultimoEstadoBotao = HIGH;
unsigned long ultimoDebounce = 0;
const unsigned long debounceDelay = 50;

// Controle de tempo do loop principal
unsigned long ultimoLoop = 0;
const unsigned long intervaloLoop = 1000;

void setup() {
  Serial.begin(57600);
  pinMode(RelePin, OUTPUT);
  pinMode(ledBLig, OUTPUT);
  pinMode(btPin, INPUT_PULLUP);

  digitalWrite(RelePin, LOW);
  digitalWrite(ledBLig, LOW);

  // Inicializa o RTC
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
    Rtc.SetDateTime(compiled);
  }
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);

  // Carrega horários da EEPROM
  hlig1 = EEPROM.read(EEPROM_ADDR_HLIG1);
  hdes1 = EEPROM.read(EEPROM_ADDR_HDES1);
  hlig2 = EEPROM.read(EEPROM_ADDR_HLIG2);
  hdes2 = EEPROM.read(EEPROM_ADDR_HDES2);

  // Se os valores da EEPROM forem inválidos, carrega valores padrão
  if (hlig1 >= 24 || hdes1 >= 24 || hlig2 >= 24 || hdes2 >= 24) {
    hlig1 = 6; hdes1 = 8;
    hlig2 = 17; hdes2 = 20;
    salvarHorariosEEPROM(); // Salva os padrões na primeira vez
  }

  Serial.println("Sistema iniciado.");
  imprimirHorarios();
}

void loop() {
  checarComandosSerial();
  verificarBotao();

  // Executa a lógica principal a cada segundo
  if (millis() - ultimoLoop >= intervaloLoop) {
    ultimoLoop = millis();
    atualizarEstadoSistema();
  }
}

// PONTO 1 CORRIGIDO: Lógica de toggle para o botão
void verificarBotao() {
  bool leitura = digitalRead(btPin);

  // Inicia o contador de debounce na primeira detecção de mudança
  if (leitura != ultimoEstadoBotao) {
    ultimoDebounce = millis();
  }

  // Após o tempo de debounce, confirma a mudança
  if ((millis() - ultimoDebounce) > debounceDelay) {
    // Se o estado do botão realmente mudou
    if (leitura != ultimoEstadoBotao) {
      ultimoEstadoBotao = leitura;
      // Se o botão foi pressionado (estado LOW)
      if (leitura == LOW) {
        blig = !blig; // Inverte o estado do override manual
        if (blig) {
          Serial.println("Override manual ATIVADO.");
        } else {
          Serial.println("Override manual DESATIVADO.");
        }
      }
    }
  }
}


void atualizarEstadoSistema() {
  RtcDateTime now = Rtc.GetDateTime();
  if (!now.IsValid()) {
    Serial.println("Erro na leitura do RTC.");
    return;
  }

  int minutoAtual = now.Hour() * 60 + now.Minute();
  bool dentro1 = (minutoAtual >= hlig1 * 60 && minutoAtual < hdes1 * 60);
  bool dentro2 = (minutoAtual >= hlig2 * 60 && minutoAtual < hdes2 * 60);
  lig = dentro1 || dentro2;

  // Lógica de segurança: desliga o modo manual em horários específicos
  if (now.Hour() == 0 || now.Hour() == 17 || now.Hour() == 22) {
    if (blig) {
      blig = false;
      Serial.println("Override manual desligado automaticamente por horário crítico.");
    }
  }

  bool estadoRele = lig || blig;
  digitalWrite(RelePin, estadoRele ? HIGH : LOW);
  digitalWrite(ledBLig, blig ? HIGH : LOW);

  // Imprime status a cada segundo
  Serial.print("Hora: ");
  printDateTime(now);
  Serial.print(" | Rele: ");
  Serial.println(estadoRele ? "LIGADO" : "DESLIGADO");
}

// PONTO 3 CORRIGIDO: Leitura serial sem a classe String
void checarComandosSerial() {
  static char comando[32];
  static byte indice = 0;

  while (Serial.available() > 0) {
    char c = Serial.read();
    c = toupper(c); // Converte para maiúscula para facilitar comparação

    if (c == '\n' || c == '\r') { // Fim do comando
      if (indice > 0) {
        comando[indice] = '\0'; // Finaliza a string C
        
        // Processa os comandos
        if (strncasecmp(comando, "SET LIG1 ", 9) == 0) {
          hlig1 = atoi(&comando[9]);
          horariosAlterados = true;
          Serial.println("OK. LIG1 alterado para " + String(hlig1) + "h. Use SALVAR para gravar.");
        } else if (strncasecmp(comando, "SET DES1 ", 9) == 0) {
          hdes1 = atoi(&comando[9]);
          horariosAlterados = true;
          Serial.println("OK. DES1 alterado para " + String(hdes1) + "h. Use SALVAR para gravar.");
        } else if (strncasecmp(comando, "SET LIG2 ", 9) == 0) {
          hlig2 = atoi(&comando[9]);
          horariosAlterados = true;
          Serial.println("OK. LIG2 alterado para " + String(hlig2) + "h. Use SALVAR para gravar.");
        } else if (strncasecmp(comando, "SET DES2 ", 9) == 0) {
          hdes2 = atoi(&comando[9]);
          horariosAlterados = true;
          Serial.println("OK. DES2 alterado para " + String(hdes2) + "h. Use SALVAR para gravar.");
        } else if (strcasecmp(comando, "STATUS") == 0) {
          imprimirHorarios();
        } else if (strcasecmp(comando, "SALVAR") == 0) { // PONTO 4 CORRIGIDO
          salvarHorariosEEPROM();
          horariosAlterados = false;
          Serial.println("OK. Horarios salvos permanentemente na EEPROM.");
        } else {
          Serial.println("Comando invalido. Use: SET LIG1/DES1/LIG2/DES2 <hora>, STATUS, SALVAR");
        }
        
        indice = 0; // Reseta para o próximo comando
      }
    } else if (indice < (sizeof(comando) - 1)) {
      comando[indice++] = c; // Adiciona caractere ao buffer
    }
  }
}

// PONTO 4 CORRIGIDO: Função para salvar é chamada apenas pelo comando SALVAR
void salvarHorariosEEPROM() {
  EEPROM.write(EEPROM_ADDR_HLIG1, hlig1);
  EEPROM.write(EEPROM_ADDR_HDES1, hdes1);
  EEPROM.write(EEPROM_ADDR_HLIG2, hlig2);
  EEPROM.write(EEPROM_ADDR_HDES2, hdes2);
}

void imprimirHorarios() {
  Serial.println("--- Status dos Horarios ---");
  Serial.print("Intervalo 1: "); Serial.print(hlig1); Serial.print("h as "); Serial.print(hdes1); Serial.println("h");
  Serial.print("Intervalo 2: "); Serial.print(hlig2); Serial.print("h as "); Serial.print(hdes2); Serial.println("h");
  if (horariosAlterados) {
    Serial.println("AVISO: Existem alteracoes nao salvas. Use o comando SALVAR.");
  }
  Serial.println("---------------------------");
}

#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime& dt) {
  char datestring[20];
  snprintf_P(datestring, countof(datestring), PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Day(), dt.Month(), dt.Year(),
             dt.Hour(), dt.Minute(), dt.Second());
  Serial.print(datestring);
}
