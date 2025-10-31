#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiSTA.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiType.h>
#include <WiFiUdp.h>
 
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "soc/gpio_struct.h"

#include <supermeatboy1-project-1_inferencing.h>

#include "command_received_audio.h"

#include "env.h"

// === CONFIG ===
#define MIC_PIN ADC1_CHANNEL_7  // GPIO35
#define SAMPLE_RATE 16000       // 16 kHz sample rate

// Circular buffer settings
#define BUFFER_SIZE 16000
volatile int16_t audioBuffer[BUFFER_SIZE];
volatile int16_t processedAudioBuffer[BUFFER_SIZE];

volatile int32_t writeIndex = 0;
volatile int32_t ttsReadIndex = 0;

volatile bool authenticatedServ = false;

bool isDiagnostic = false;

#define VAD_THRESHOLD 5000
#define VAD_MIN_COUNT 10
#define VAD_OFFSET_BYTES 1024

hw_timer_t* timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

hw_timer_t* ttsTimer = NULL;
portMUX_TYPE ttsTimerMux = portMUX_INITIALIZER_UNLOCKED;

#define OE_ON       GPIO.out_w1tc = (1 << 26);
#define OE_OFF      GPIO.out_w1ts = (1 << 26);
#define SER_ON      GPIO.out_w1ts = (1 << 27);
#define SER_OFF     GPIO.out_w1tc = (1 << 27);
#define RCLK_ON     GPIO.out_w1ts = (1 << 14);
#define RCLK_OFF    GPIO.out_w1tc = (1 << 14);
#define SRCLK_ON    GPIO.out_w1ts = (1 << 12);
#define SRCLK_OFF   GPIO.out_w1tc = (1 << 12);

// ADC characteristics
esp_adc_cal_characteristics_t adc_chars;

int16_t adc_to_short(int input) {
  int32_t centered = (int32_t) input - 540;

  int32_t scaled = centered * 32;
  scaled -= 19264;
  scaled += 11445;
  scaled += 3626;

  // Clamp in case of slight overflow when x == 4096
  if (scaled > 32767) scaled = 32767;
  if (scaled < -32767) scaled = -32767;

  return (int16_t) scaled;
}

// === ISR Timer Handler ===
void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);

    // Read raw ADC data as fast as possible
    int raw = adc1_get_raw(MIC_PIN);

    // Convert ADC value (0-4095) → signed range -2048 to 2047
    int16_t centered = (int16_t) adc_to_short(raw);

    audioBuffer[writeIndex] = centered;
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;

    portEXIT_CRITICAL_ISR(&timerMux);
}

// === ISR TTS Timer Handler ===
void IRAM_ATTR onTtsTimer() {
    portENTER_CRITICAL_ISR(&ttsTimerMux);

    uint8_t sample = command_received_audio_raw[ttsReadIndex];

    // Turn Latch pin off to prevent changes in the speaker side.
    OE_OFF;
    RCLK_OFF;
    for (int8_t i = 0; i < 8; i++) {
      if (sample & (1 << i)) {
        SER_ON;
      } else {
        SER_OFF;
      }
      SRCLK_ON;
      SRCLK_OFF;
    }
    RCLK_ON;
    RCLK_OFF;
    OE_ON;

    if (ttsReadIndex + 1 >= sizeof(command_received_audio_raw)) {
      timerStop(ttsTimer);
      timerRestart(ttsTimer);
    }
    ttsReadIndex = (ttsReadIndex + 1) % sizeof(command_received_audio_raw);

    portEXIT_CRITICAL_ISR(&ttsTimerMux);
}

void setupADC() {
    // Configure ADC1, channel 7 (GPIO35)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(MIC_PIN, ADC_ATTEN_DB_11); 
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12, 1100, &adc_chars);
}

void setupTimer() {
    timer = timerBegin(SAMPLE_RATE);
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 1, true, 0);

    ttsTimer = timerBegin(SAMPLE_RATE);
    timerAttachInterrupt(ttsTimer, &onTtsTimer);
    timerAlarm(ttsTimer, 1, true, 0);
    timerStop(ttsTimer);
    timerRestart(ttsTimer);
}

WiFiClient netClient;

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI__SSID, WIFI__PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();

  Serial.println("Local IP address: ");
  Serial.println(WiFi.localIP());
}

inline bool fastRead34() {
    return (GPIO.in1.val & (1UL << 2)) != 0;
}
static inline bool fastReadGPIO32() {
    return (REG_READ(GPIO_IN1_REG) & (1UL << 0)) != 0;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Starting ADC interrupt sampler...");

    pinMode(34, INPUT);
    pinMode(32, INPUT);

    pinMode(26, OUTPUT);
    pinMode(27, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(12, OUTPUT);

    setupADC();
    setupTimer();
    setupWifi();
}

unsigned char* short_to_bytes(int16_t n) {
  static unsigned char bytes[2];
  bytes[1] = (n >> 8) & 0xFF;
  bytes[0] = n & 0xFF;
  return bytes;
}

unsigned char* int_to_bytes(int n) {
  static unsigned char bytes[4];
  bytes[3] = (n >> 24) & 0xFF;
  bytes[2] = (n >> 16) & 0xFF;
  bytes[1] = (n >> 8) & 0xFF;
  bytes[0] = n & 0xFF;
  return bytes;
}
void rotateAndZero(int16_t *buf, int bufSize, int startIndex) {
    if (startIndex <= 0 || startIndex >= bufSize) return;

    // Step 1: reverse first part
    // [A B C | D E F] startIndex=3
    // --> [C B A | D E F]
    for (int i = 0, j = startIndex - 1; i < j; i++, j--) {
        int16_t tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
    }

    // Step 2: reverse second part
    // --> [C B A | F E D]
    for (int i = startIndex, j = bufSize - 1; i < j; i++, j--) {
        int16_t tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
    }

    // Step 3: reverse whole buffer
    // --> [D E F A B C]
    for (int i = 0, j = bufSize - 1; i < j; i++, j--) {
        int16_t tmp = buf[i];
        buf[i] = buf[j];
        buf[j] = tmp;
    }

    // Step 4: zero out the trailing area (old pre-speech data)
    int validLen = bufSize - startIndex;
    memset(buf + validLen, 0, startIndex * sizeof(int16_t));
}


void getContiguousFrame() {
    int startIndex = writeIndex - BUFFER_SIZE;
    if (startIndex < 0) {
        startIndex += BUFFER_SIZE;
    }

    // Chunk sizes
    int firstChunk = BUFFER_SIZE - startIndex;
    if (firstChunk > BUFFER_SIZE) {
        firstChunk = BUFFER_SIZE;
    }

    int secondChunk = BUFFER_SIZE - firstChunk;

    // Copy chunk 1: from startIndex to end of buffer
    memcpy((void*) processedAudioBuffer,
           (void*) &audioBuffer[startIndex],
           firstChunk * sizeof(int16_t));

    // Copy chunk 2: from start of buffer to writeIndex
    if (secondChunk > 0) {
        memcpy((void*) processedAudioBuffer + (sizeof(int16_t) * firstChunk),
               (void*) audioBuffer,
               secondChunk * sizeof(int16_t));
    }

    // Align audio to begin where the word begins

    // ---- Bare MINIMUM!!! VAD ----
    int pVadCounter = 0;
    int pSpeechIndex = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
      if (abs(processedAudioBuffer[i]) > VAD_THRESHOLD) {
          pVadCounter++;
          if (pVadCounter >= VAD_MIN_COUNT) {
            pSpeechIndex = i - VAD_OFFSET_BYTES;
            if (pSpeechIndex < 0) {
              pSpeechIndex = 0;
            }
            break;
          }
      }
    }

    if (pSpeechIndex > 0) {
      //Serial.println("pSpeechIndex: " + String(pSpeechIndex));
      rotateAndZero((int16_t*) processedAudioBuffer, BUFFER_SIZE, pSpeechIndex);
    }
}

static int microphone_audio_signal_get_data(size_t offset,
        size_t length, float *out_ptr) {
    numpy::int16_to_float((const int16_t*) &processedAudioBuffer[offset], out_ptr, length);
    return 0;
}

static String strOut = "";

String infer() {
    signal_t signal;
    signal.total_length = BUFFER_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    strOut = "";

    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);
    if (r != EI_IMPULSE_OK) return "";

    // Skip if there is too much background noise.
    //if (result.classification[0].value > 0.45) {
    //  return;
    //}
    //Serial.printf("\nPredictions:\n");
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        strOut += result.classification[i].label;
        strOut += ":";
        strOut += result.classification[i].value;
        if (i < EI_CLASSIFIER_LABEL_COUNT - 1) {
          strOut += "\n";
        }
        Serial.printf("%s:%.3f",
            result.classification[i].label,
            result.classification[i].value);
        if (i != EI_CLASSIFIER_LABEL_COUNT - 1) {
          Serial.print(",");
        }
    }

    if (result.classification[0].value < 0.6 && (result.classification[1].value > 0.7 || result.classification[4].value > 0.7)) {
      timerStart(ttsTimer);
    }

    Serial.println();

    return strOut;
}

void diagnostic() {
    isDiagnostic = true;
    // Periodically print buffer status
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 100) {
        lastPrint = millis();

        portENTER_CRITICAL(&timerMux);
        uint16_t indexCopy = writeIndex;
        portEXIT_CRITICAL(&timerMux);

        if (fastReadGPIO32()) {
          Serial.println("I[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");
          timerStop(timer);
          Serial.print('I');
          getContiguousFrame();

          infer();
          timerStart(timer);
        }
        //Serial.print("Buffer write index: ");
        //Serial.println(indexCopy);

        if (fastRead34()) {
          timerStop(timer);
          //Serial.print('I');
          getContiguousFrame();
          Serial.print('D');
          Serial.print(">");
          unsigned char* sample_len_bytes = int_to_bytes(BUFFER_SIZE);
          for (int i = 0; i < 4; i++) {
            Serial.write(sample_len_bytes[i]);
          }

          for (int i = 0; i < BUFFER_SIZE; i++) {
            unsigned char* sample_bytes = short_to_bytes(audioBuffer[i]);
            Serial.write(sample_bytes[0]);
            Serial.write(sample_bytes[1]);
          }

          /*
          int endIndex = writeIndex - 1;
          if (endIndex < 0) {
            endIndex = BUFFER_SIZE - 1;
          }
          int i = writeIndex;
          while (i != endIndex) {
            unsigned char* sample_bytes = short_to_bytes(audioBuffer[i]);
            Serial.write(sample_bytes[0]);
            Serial.write(sample_bytes[1]);
            i++;
            if (i >= BUFFER_SIZE) { i = 0; }
          }
          */

          /*
          for (int i = 0; i < BUFFER_SIZE; i++) {
            unsigned char* sample_bytes = short_to_bytes(processedAudioBuffer[i]);
            Serial.write(sample_bytes[0]);
            Serial.write(sample_bytes[1]);
          }
          */

          timerStart(timer);
        }
    }
}

void loop() {
    // Periodically print buffer status
    //diagnostic();

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1500) {
        Serial.println("I[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");
        //timerStop(timer);
        getContiguousFrame();

        infer();
        //timerStart(timer);

        //timerStop(timer);
        //timerStop(ttsTimer);
        //timerStart(ttsTimer);
        //timerStart(timer);
        lastPrint = millis();

        if (fastRead34()) {
          timerStart(ttsTimer);
        }
        
        if (!netClient.connected()) {
          Serial.println("Connecting.");
          netClient.connect(SERVER__HOST, SERVER__PORT);
        }
        if (netClient.connected()) {
          if (!authenticatedServ) {
            Serial.println("Sending secret.");
            netClient.println(SERVER__SECRET);
            delay(1000);
            authenticatedServ = true;
          } else if (strOut.length() > 3) {
            Serial.println("Sending ordinary data.");
            //Serial.println(strOut.c_str());
            netClient.println("=== WakeAlert-ESP32 LOGIN ===");
            netClient.println(strOut.c_str());
          }
        }
    }
}
