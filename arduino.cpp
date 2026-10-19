#include <DHT.h>

// ================================
// PINOS
// ================================

#define DHT_PIN 2
#define DHT_TYPE DHT11

#define CHAMA_PIN 3
#define VIBRACAO_PIN 4
#define BOTAO_PIN 6

#define RELE_PIN 7
#define BUZZER_PIN 8

#define LED_R 9
#define LED_G 10
#define LED_B 11

#define LDR_PIN A1

// ================================
// CONFIGURACAO
// ================================

// Ajuste depois de testar seu modulo.
const int RELE_LIGADO = LOW;
const int RELE_DESLIGADO = HIGH;

// Ajuste conforme a leitura real do sensor.
const int LIMITE_LDR = 500;

// No KY-026, LOW frequentemente indica deteccao,
// mas confirme o comportamento do seu modulo.
const int CHAMA_DETECTADA = LOW;

// Botao ligado entre D6 e GND.
DHT dht(DHT_PIN, DHT_TYPE);

// ================================
// ESTADOS
// ================================

enum Estado {
  NORMAL,
  ALERTA,
  EMERGENCIA,
  BLOQUEADO_NR12
};

Estado estado = NORMAL;

bool bloqueioMemorizado = false;
bool barreiraInterrompida = false;
bool chamaDetectada = false;

float temperatura = 0;
float umidade = 0;

unsigned long ultimaLeitura = 0;
unsigned long ultimoPulso = 0;

volatile unsigned long totalVibracoes = 0;

const unsigned long INTERVALO_LEITURA = 1000;
const unsigned long INTERVALO_VIBRACAO = 100;

// ================================
// INTERRUPCAO DE VIBRACAO
// ================================

void contarVibracao() {
  unsigned long agora = millis();

  if (agora - ultimoPulso >= INTERVALO_VIBRACAO) {
    totalVibracoes++;
    ultimoPulso = agora;
  }
}

// ================================
// LED RGB
// ================================

void ledVerde() {
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

void ledAmarelo() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, LOW);
}

void ledVermelho() {
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_B, LOW);
}

// ================================
// ATUADORES
// ================================

void desligarMaquina() {
  digitalWrite(RELE_PIN, RELE_DESLIGADO);
}

void ligarMaquina() {
  digitalWrite(RELE_PIN, RELE_LIGADO);
}

void ligarAlarme() {
  digitalWrite(BUZZER_PIN, HIGH);
}

void desligarAlarme() {
  digitalWrite(BUZZER_PIN, LOW);
}

// ================================
// SETUP
// ================================

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(CHAMA_PIN, INPUT);
  pinMode(VIBRACAO_PIN, INPUT);

  // Botao entre D6 e GND.
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  pinMode(RELE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Estado seguro durante a inicializacao.
  desligarMaquina();
  desligarAlarme();
  ledVermelho();

  attachInterrupt(
    digitalPinToInterrupt(VIBRACAO_PIN),
    contarVibracao,
    RISING
  );

  Serial.println("SmartFactory SafeGuard iniciado.");
  Serial.println("Aguardando verificacao dos sensores.");
}

// ================================
// LOOP PRINCIPAL
// ================================

void loop() {
  unsigned long agora = millis();

  if (agora - ultimaLeitura < INTERVALO_LEITURA) {
    return;
  }

  ultimaLeitura = agora;

  // 1. Ler temperatura e umidade.
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("ERRO: falha na leitura do DHT11.");

    // Falha de sensor: adotar estado seguro.
    bloqueioMemorizado = true;
    estado = EMERGENCIA;
    desligarMaquina();
    ligarAlarme();
    ledVermelho();

    return;
  }

  temperatura = t;
  umidade = h;

  // 2. Ler chama.
  chamaDetectada =
    digitalRead(CHAMA_PIN) == CHAMA_DETECTADA;

  // 3. Ler barreira.
  int valorLDR = analogRead(LDR_PIN);

  // Esta condicao depende da calibracao.
  barreiraInterrompida = valorLDR < LIMITE_LDR;

  // 4. Ler vibracoes.
  unsigned long vibracoes;

  noInterrupts();
  vibracoes = totalVibracoes;
  interrupts();

  // 5. Ler botao de rearme.
  bool botaoPressionado =
    digitalRead(BOTAO_PIN) == LOW;

  // 6. Identificar emergencia.
  bool temperaturaCritica = temperatura > 30.0;

  bool emergencia =
    temperaturaCritica ||
    chamaDetectada ||
    barreiraInterrompida;

  if (emergencia) {
    bloqueioMemorizado = true;
  }

  // 7. Rearme manual.
  // So aceita rearme quando a causa da emergencia
  // nao estiver mais presente.
  if (botaoPressionado && !emergencia) {
    bloqueioMemorizado = false;
  }

  // 8. Definir estado operacional.
  if (bloqueioMemorizado) {
    estado = BLOQUEADO_NR12;
  } else if (
    vibracoes >= 5 ||
    umidade < 40.0 ||
    umidade > 70.0
  ) {
    estado = ALERTA;
  } else {
    estado = NORMAL;
  }

  // 9. Controlar atuadores.
  switch (estado) {
    case NORMAL:
      ligarMaquina();
      ledVerde();
      desligarAlarme();
      break;

    case ALERTA:
      ligarMaquina();
      ledAmarelo();
      ligarAlarme();
      break;

    case EMERGENCIA:
    case BLOQUEADO_NR12:
      desligarMaquina();
      ledVermelho();
      ligarAlarme();
      break;
  }

  // 10. Mostrar dados no Monitor Serial.
  Serial.println("--------------------------");

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" C");

  Serial.print("Umidade: ");
  Serial.print(umidade);
  Serial.println(" %");

  Serial.print("Valor LDR: ");
  Serial.println(valorLDR);

  Serial.print("Chama: ");
  Serial.println(chamaDetectada ? "SIM" : "NAO");

  Serial.print("Vibracoes acumuladas: ");
  Serial.println(vibracoes);

  Serial.print("Estado: ");

  switch (estado) {
    case NORMAL:
      Serial.println("NORMAL");
      break;

    case ALERTA:
      Serial.println("ALERTA");
      break;

    case EMERGENCIA:
      Serial.println("EMERGENCIA");
      break;

    case BLOQUEADO_NR12:
      Serial.println("BLOQUEADO_NR12");
      break;
  }
}