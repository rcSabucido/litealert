#include <driver/adc.h>
#include "esp_adc_cal.h"
#include "soc/gpio_struct.h"

#include <supermeatboy1-project-1_inferencing.h>

// === CONFIG ===
#define MIC_PIN ADC1_CHANNEL_7  // GPIO35
#define SAMPLE_RATE 16000       // 16 kHz sample rate

// Circular buffer settings
#define BUFFER_SIZE 16000
volatile int16_t audioBuffer[BUFFER_SIZE];
volatile int16_t processedAudioBuffer[BUFFER_SIZE];
volatile uint16_t writeIndex = 0;

#define VAD_THRESHOLD 5000
#define VAD_MIN_COUNT 10
#define VAD_OFFSET_BYTES 1024

hw_timer_t* timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// ADC characteristics
esp_adc_cal_characteristics_t adc_chars;

int16_t adc_to_short(int input) {
  int32_t centered = (int32_t) input - 2048;

  int32_t scaled = centered * 16;
  scaled += 5000;

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

    setupADC();
    setupTimer();
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

void infer() {
    signal_t signal;
    signal.total_length = BUFFER_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);
    if (r != EI_IMPULSE_OK) return;

    // Skip if there is too much background noise.
    if (result.classification[0].value > 0.45) {
      return;
    }
    //Serial.printf("\nPredictions:\n");
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.printf("%s:%.3f",
            result.classification[i].label,
            result.classification[i].value);
        if (i != EI_CLASSIFIER_LABEL_COUNT - 1) {
          Serial.print(",");
        }
    }
    Serial.println();
}

void diagnostic() {
    // Periodically print buffer status
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 100) {
        lastPrint = millis();

        portENTER_CRITICAL(&timerMux);
        uint16_t indexCopy = writeIndex;
        portEXIT_CRITICAL(&timerMux);

        if (fastReadGPIO32()) {
          //Serial.println("I[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");
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
          Serial.print('I');
          getContiguousFrame();
          Serial.print('D');
          Serial.print(">");
          unsigned char* sample_len_bytes = int_to_bytes(BUFFER_SIZE);
          for (int i = 0; i < 4; i++) {
            Serial.write(sample_len_bytes[i]);
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

          for (int i = 0; i < BUFFER_SIZE; i++) {
            unsigned char* sample_bytes = short_to_bytes(processedAudioBuffer[i]);
            Serial.write(sample_bytes[0]);
            Serial.write(sample_bytes[1]);
          }

          timerStart(timer);
        }
    }
}

void loop() {
    // Periodically print buffer status
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 1500) {
        //Serial.println("I[APP] Free memory: " + String(esp_get_free_heap_size()) + " bytes");
        //timerStop(timer);
        getContiguousFrame();

        infer();
        //timerStart(timer);
    }
}
