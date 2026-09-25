/*
 * ECCS - Sprint 3 - Edge Computing & Computer Systems
 * Monitoramento de condições ambientais de treinamento
 * Pelé Academia (Resende) - campos abertos
 *
 * Hardware:
 *   - ESP32
 *   - Sensor DHT22 (temperatura + umidade)
 *   - Display LCD 16x2 (I2C)
 *
 * Edge (no ESP32):
 *   - Aquisição periódica de temperatura e umidade
 *   - Cálculo de média móvel das últimas N leituras
 *   - Cálculo do Índice de Calor (Heat Index - modelo NWS/Rothfusz)
 *   - Classificação do risco térmico (Normal / Atenção / Perigo / Perigo Extremo)
 *   - Exibição imediata no display LCD
 *
 * Cloud (ThingSpeak):
 *   - Armazenamento histórico
 *   - Visualização gráfica remota
 *   - Acompanhamento público do canal
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------------- CONFIGURAÇÕES DE HARDWARE ----------------------

#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// ---------------------- CONFIGURAÇÕES DE REDE ----------------------

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ThingSpeak
const char* TS_API_KEY = "SUA_WRITE_API_KEY_AQUI";
const char* TS_SERVER = "http://api.thingspeak.com/update";

// ---------------------- PARÂMETROS DE PROCESSAMENTO LOCAL (EDGE) ----------------------

const int JANELA_MEDIA = 5;          // quantidade de leituras usadas na média móvel
float bufferTemp[JANELA_MEDIA];
float bufferUmid[JANELA_MEDIA];
int indiceBuffer = 0;
int leiturasValidas = 0;

const unsigned long INTERVALO_LEITURA_MS = 5000;   // leitura local a cada 5s
const unsigned long INTERVALO_ENVIO_MS   = 16000;   // envio ao ThingSpeak a cada 16s (limite free ~15s)

unsigned long ultimaLeitura = 0;
unsigned long ultimoEnvio = 0;

// Faixas de classificação de risco térmico (Heat Index, °C)
// Baseado no modelo do National Weather Service, adaptado para contexto
// de atividade física intensa ao ar livre (treino esportivo em campo aberto).
enum NivelRisco { NORMAL, ATENCAO, PERIGO, PERIGO_EXTREMO };

// ---------------------- SETUP ----------------------

void setup() {
  Serial.begin(115200);
  dht.begin();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Pele Academia");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  conectarWiFi();
}

// ---------------------- LOOP PRINCIPAL ----------------------

void loop() {
  unsigned long agora = millis();

  // --- Aquisição + processamento local (edge), a cada INTERVALO_LEITURA_MS ---
  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;

    float temp = dht.readTemperature();
    float umid = dht.readHumidity();

    if (isnan(temp) || isnan(umid)) {
      Serial.println("Falha na leitura do DHT22");
      exibirErroSensor();
      return;
    }

    // Atualiza buffer circular (processamento local - média móvel)
    bufferTemp[indiceBuffer] = temp;
    bufferUmid[indiceBuffer] = umid;
    indiceBuffer = (indiceBuffer + 1) % JANELA_MEDIA;
    if (leiturasValidas < JANELA_MEDIA) leiturasValidas++;

    float mediaTemp = calcularMedia(bufferTemp, leiturasValidas);
    float mediaUmid = calcularMedia(bufferUmid, leiturasValidas);

    // Processamento local - índice de calor e classificação de risco
    float heatIndex = calcularHeatIndex(mediaTemp, mediaUmid);
    NivelRisco risco = classificarRisco(heatIndex);

    // Apresentação imediata na borda (IHM local)
    exibirLCD(temp, umid, mediaTemp, mediaUmid, heatIndex, risco);

    Serial.printf(
      "Temp: %.1fC | Umid: %.1f%% | MediaTemp: %.1fC | MediaUmid: %.1f%% | HeatIndex: %.1fC | Risco: %s\n",
      temp, umid, mediaTemp, mediaUmid, heatIndex, nomeRisco(risco)
    );

    // --- Envio para nuvem (ThingSpeak), respeitando intervalo próprio ---
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
      ultimoEnvio = agora;
      enviarThingSpeak(mediaTemp, mediaUmid, heatIndex, risco);
    }
  }
}

// ---------------------- FUNÇÕES DE PROCESSAMENTO LOCAL (EDGE) ----------------------

float calcularMedia(float* buffer, int n) {
  float soma = 0;
  for (int i = 0; i < n; i++) soma += buffer[i];
  return soma / n;
}

// Índice de calor (Heat Index) - fórmula de Rothfusz (NWS), entrada em Fahrenheit
float calcularHeatIndex(float tempC, float umidPct) {
  float T = tempC * 9.0 / 5.0 + 32.0; // Celsius -> Fahrenheit
  float R = umidPct;

  float HI = -42.379 + 2.04901523 * T + 10.14333127 * R
             - 0.22475541 * T * R - 0.00683783 * T * T
             - 0.05481717 * R * R + 0.00122874 * T * T * R
             + 0.00085282 * T * R * R - 0.00000199 * T * T * R * R;

  // Para condições de temperatura mais amenas, aproximação simplificada
  // (evita valores irreais em cenários fora da faixa de validade da fórmula completa)
  if (T < 80) {
    HI = 0.5 * (T + 61.0 + (T - 68.0) * 1.2 + R * 0.094);
  }

  return (HI - 32.0) * 5.0 / 9.0; // Fahrenheit -> Celsius
}

NivelRisco classificarRisco(float heatIndexC) {
  if (heatIndexC < 32.0) return NORMAL;
  if (heatIndexC < 41.0) return ATENCAO;
  if (heatIndexC < 54.0) return PERIGO;
  return PERIGO_EXTREMO;
}

const char* nomeRisco(NivelRisco r) {
  switch (r) {
    case NORMAL: return "NORMAL";
    case ATENCAO: return "ATENCAO";
    case PERIGO: return "PERIGO";
    case PERIGO_EXTREMO: return "PERIGO EXTREMO";
  }
  return "?";
}

// ---------------------- IHM LOCAL (LCD) ----------------------

// LCD 16x2 tem pouco espaço: alterna entre duas "telas" a cada leitura.
bool telaAlternada = false;

void exibirLCD(float temp, float umid, float mediaTemp, float mediaUmid,
               float heatIndex, NivelRisco risco) {
  lcd.clear();

  if (!telaAlternada) {
    // Tela 1: leitura atual + média
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C U:");
    lcd.print(umid, 0);
    lcd.print("%");

    lcd.setCursor(0, 1);
    lcd.print("Med ");
    lcd.print(mediaTemp, 1);
    lcd.print("C ");
    lcd.print(mediaUmid, 0);
    lcd.print("%");
  } else {
    // Tela 2: índice de calor + status de risco
    lcd.setCursor(0, 0);
    lcd.print("Indice: ");
    lcd.print(heatIndex, 1);
    lcd.print("C");

    lcd.setCursor(0, 1);
    lcd.print("Status: ");
    lcd.print(nomeRisco(risco));
  }
  telaAlternada = !telaAlternada;
}

void exibirErroSensor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Erro sensor");
  lcd.setCursor(0, 1);
  lcd.print("DHT22");
}

// ---------------------- CONECTIVIDADE E ENVIO À NUVEM ----------------------

void conectarWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nFalha ao conectar ao WiFi");
  }
}

void enviarThingSpeak(float mediaTemp, float mediaUmid, float heatIndex, NivelRisco risco) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado - tentando reconectar...");
    conectarWiFi();
    return;
  }

  HTTPClient http;
  String url = String(TS_SERVER) + "?api_key=" + TS_API_KEY +
               "&field1=" + String(mediaTemp, 1) +
               "&field2=" + String(mediaUmid, 1) +
               "&field3=" + String(heatIndex, 1) +
               "&field4=" + String((int)risco);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.printf("ThingSpeak atualizado. Codigo: %d\n", httpCode);
  } else {
    Serial.printf("Erro ao enviar para ThingSpeak: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
