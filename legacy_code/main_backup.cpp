#include <Arduino.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include "BluetoothA2DPSink.h"
#include "esp_dsp.h"
#include <esp_heap_caps.h>

#define DEBUG

BluetoothA2DPSink a2dp_sink;
bool detecting = true;

void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
}

// Global variables to hold cumulative data
int cumulative_zero_crossings = 0;
uint32_t cumulative_sample_count = 0;
const float sampling_rate = 48000.0; // Sampling rate in Hz
const int16_t threshold = 1000; // Example threshold, adjust based on your signal
const uint32_t continuous_low_threshold_samples = 10; // Number of consecutive samples below threshold to consider as end of sine wave


bool is_sine_wave = false; // State to track if currently in a sine wave part
int16_t previous_sample = 0; // Hold the previous sample for zero crossing detection
uint32_t low_threshold_count = 0; // Counter to track consecutive samples below threshold

// Array to hold detected frequencies
#define MAX_FREQUENCIES 50
#define MAX_MESSAGES 100
float frequencies[MAX_FREQUENCIES];
char decoded_message[MAX_FREQUENCIES + 1]; // Plus one for null-terminator
uint8_t frequency_count = 0;

void reset_detection() {
    is_sine_wave = false;
    low_threshold_count = 0;
    cumulative_zero_crossings = 0;
    cumulative_sample_count = 0;
    detecting = true;
}

// Function to decode frequency to character
char decode_binary_frequency(float frequency) {
    if (frequency >= 750 && frequency <= 1250) {
        return '0';
    } else if (frequency >= 1750 && frequency <= 2250) {
        return '1';
    } else {
        return '?'; // Unknown frequency
    }
}

// Base frequency interval
const int frequencyBase = 150;

// Function to decode frequency to character
char decode_ascii_frequency(float frequency) {
    // Convert frequency to ASCII code using the base frequency interval
    int asciiCode = static_cast<int>(frequency / frequencyBase);

    // Check if the ASCII code is within the valid range
    if (asciiCode >= 32 && asciiCode <= 126) {
        return static_cast<char>(asciiCode);
    } else {
        return '?'; // Unknown frequency
    }
}


std::vector<std::string> extract_data(const std::string& array, int chunk_size) {
    // remove questionmark
    std::string cleaned_array;
    for (char c : array) {
        if (c != '?') {
            cleaned_array += c;
        }
    }

    // define the decoding pattern
    std::regex pattern("1+0*|0+1*");
    std::sregex_iterator iter(cleaned_array.begin(), cleaned_array.end(), pattern);
    std::sregex_iterator end;

    if (iter == end) {
        throw std::invalid_argument("No valid pattern found in the array");
    }

    // caculate the interval
    int interval = 2400 / chunk_size;

    // extract data
    std::vector<std::string> result;
    for (size_t i = 0; i < cleaned_array.length(); i += interval) {
        std::string segment = cleaned_array.substr(i, interval);
        if (!segment.empty()) {
            result.push_back(segment);
        }
    }

    return result;
}

// Function to check the growth rate ratio
void check_ratio(uint32_t &sample_count_for_current_freq, int &previous_sample_count) {
    // Calculate the growth since last check
    int sample_growth = sample_count_for_current_freq - previous_sample_count;
    // Update the previous values
    previous_sample_count = sample_count_for_current_freq;

    // Check the ratio
    if (sample_growth != 0) {
        Serial.print("Sample growth: ");
        Serial.println(sample_growth);
        // Warn the change
    }
}

void detect_start(int16_t left_sample, int16_t right_sample, int &consecutiveCount, const int requiredConsecutive) {
    if (left_sample > right_sample) {
        consecutiveCount++;
    } else {
        consecutiveCount = 0; // Reset counter if condition is not met
    }

    // Check if we have reached the required number of consecutive occurrences
    if (consecutiveCount >= requiredConsecutive) {
        Serial.println("Audio trend detected: Left is consistently greater than Right for 40 samples.");
        detecting = false;
    }
}

void read_binary_data_frequency_onechannel(const uint8_t *data, uint32_t length) {
    // Convert byte data to 16-bit samples
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 4; // Each channel's sample is 2 bytes, total 4 bytes for both channels

    // Local variable to hold zero-crossing count for the current chunk
    int zero_crossings = 0;
    uint32_t sample_count_for_current_freq = 0;

    // Activation and ending flags settings
    int consecutiveCount = 0; // Tracker for consecutive left is greater than right
    const int requiredConsecutive = 40; // Number of required consecutive times left must be greater than right

    // Detect the frequency change
    int previous_sample_count = 0;
    int previous_zero_crossings = 0;

    Serial.println("Stream received:");
    for (uint32_t i = 0; i < sample_count; i++) {
        int16_t left_sample = samples[2 * i]; // Extract left channel sample
        int16_t right_sample = samples[2 * i + 1];

#ifdef DEBUG
        Serial.print("Sample ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(left_sample);
        Serial.print("\t");
        Serial.println(right_sample);
#endif

        // Find activation
        if (detecting) {
            detect_start(left_sample, right_sample, consecutiveCount, requiredConsecutive);
        } else {
            // Identify the start of the sine wave part
            if (!is_sine_wave && abs(left_sample) > threshold) {
                is_sine_wave = true;
                cumulative_sample_count++; // Count the first sample as part of the sine wave
                sample_count_for_current_freq++;
                previous_sample = left_sample;
                continue; // Skip the first sample to start counting from the next one
            }

            if (is_sine_wave) {
                cumulative_sample_count++; // Count samples as part of the sine wave
                sample_count_for_current_freq++;

                // Zero crossing detection
                if ((previous_sample >= 0 && left_sample < 0) || (previous_sample < 0 && left_sample >= 0)) {
                    zero_crossings++;
                    check_ratio(sample_count_for_current_freq, previous_sample_count);
                }

                previous_sample = left_sample;

                // Check if sample is below the threshold
                if (abs(left_sample) <= threshold) {
                    low_threshold_count++;
                    // If enough consecutive samples are below the threshold, consider the sine wave part ended
                    if (low_threshold_count >= continuous_low_threshold_samples) {
                        is_sine_wave = false;
                        Serial.println("Ending flags detected");
                        // Print the final decoded message
                        Serial.print("Final Decoded message: ");
                        Serial.println(decoded_message);
                        // Reset for the next frequency segment
                        reset_detection();
                    }
                } else {
                    low_threshold_count = 0; // Reset the counter if a sample exceeds the threshold
                }
            }
        }
    }

    // Final frequency calculation if still in a sine wave part
    if (is_sine_wave && sample_count_for_current_freq > 0) {
        float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count_for_current_freq);
        if (frequency_count < MAX_FREQUENCIES) {
            frequencies[frequency_count++] = estimated_frequency;
        }
        Serial.print("Detected frequency: ");
        Serial.println(estimated_frequency);
    }

    // Decode frequencies to message
    for (uint8_t i = 0; i < frequency_count; i++) {
        decoded_message[i] = decode_ascii_frequency(frequencies[i]);
    }
    decoded_message[frequency_count] = '\0'; // Null-terminate the string

    // Print the decoded message
    Serial.print("Decoded message: ");
    Serial.println(decoded_message);
}

#define SAMPLES 512  // Number of samples for FFT. Must be a power of 2.
#define SAMPLING_RATE 48000  // Sampling rate in Hz.

float real[SAMPLES_TEST];   // Array to store real values for FFT.
float imag[SAMPLES_TEST];   // Array to store imaginary values for FFT.

void read_data_frequency_fft_onechannel(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2; // Each channel's sample is 2 bytes, total 4 bytes for both channels

    if (sample_count > SAMPLES_TEST) {
        sample_count = SAMPLES_TEST;
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        real[i] = samples[2 * i];  // Assuming mono channel for simplicity
        imag[i] = 0.0;
    }

    if (dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
        Serial.println("FFT initialization failed");
        return;
    }
    // Perform FFT
    dsps_fft2r_fc32(real, SAMPLES_TEST);
    dsps_bit_rev_fc32(real, SAMPLES_TEST);
    dsps_cplx2reC_fc32(real, SAMPLES_TEST);

    // Calculate magnitude and find the peak frequency
    float max_mag = 0.0;
    int max_index = 0;
    for (int i = 0; i < SAMPLES_TEST / 2; i++) {
        float mag = sqrt(real[i] * real[i] + imag[i] * imag[i]);
        if (mag > max_mag) {
            max_mag = mag;
            max_index = i;
        }
    }

    // Calculate frequency
    float frequency = (float)max_index * SAMPLING_RATE / SAMPLES_TEST;
    Serial.print("Detected frequency: ");
    Serial.println(frequency);

    // Decode frequency to message (placeholder, implement your logic)
    decoded_message[0] = decode_ascii_frequency(frequency);
    decoded_message[1] = '\0';  // Null-terminate the string

    Serial.print("Decoded message: ");
    Serial.println(decoded_message);
}
 
void read_data_stream(const uint8_t *data, uint32_t length)
{
  int16_t *samples = (int16_t*) data;
  uint32_t sample_count = length/2;

  const float sampling_rate = 48000; // Sampling rate in Hz

  // Simple zero-crossing based frequency detection
  int zero_crossings = 0;

  Serial.println("stream received");
  Serial.println("Stream received:");
  for (uint32_t i = 0; i < sample_count; i++) {
    Serial.print("Sample ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(samples[i]);
    if (i > 0) {
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            zero_crossings++;
        }
    }
  }
  // Calculate frequency
  float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count);
  Serial.print("Estimated frequency: ");
  Serial.println(estimated_frequency);
}

void decode_fsk(const uint8_t *data, uint32_t length)
{
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2;

    // FSK parameters
    const float threshold_frequency = 1000.0; // Frequency threshold to distinguish between 0 and 1
    const float sampling_rate = 8000.0; // Sampling rate in Hz

    Serial.println("Decoding FSK stream:");
    
    // Simple zero-crossing based frequency detection
    int zero_crossings = 0;
    for (uint32_t i = 1; i < sample_count; i++) {
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            zero_crossings++;
        }
    }

    // Calculate frequency
    float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count);

    // Decode data
    String decoded_data = "";
    if (estimated_frequency > threshold_frequency) {
        decoded_data = "1";
    } else {
        decoded_data = "0";
    }

    Serial.print("Estimated frequency: ");
    Serial.println(estimated_frequency);
    //Serial.print("Decoded data: ");
    //Serial.println(decoded_data);
}

void detect_data_stream(const uint8_t *data, uint32_t length) {
  if(detecting) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2; 

    int consecutiveCount = 0; // Tracker for consecutive left is greater than right
    const int requiredConsecutive = 40; // Number of required consecutive times left must be greater than right

    Serial.println("Stream received");
    for (uint32_t i = 0; i < sample_count - 1; i += 2) { // Increment by 2 to handle left-right pairs
        int16_t leftSample = samples[i];
        int16_t rightSample = samples[i + 1];

        Serial.print("Sample ");
        Serial.print(i / 2);
        Serial.print(": Left = ");
        Serial.print(leftSample);
        Serial.print(", Right = ");
        Serial.println(rightSample);

        // Check if the left channel sample is greater than the right channel sample
        if (leftSample > rightSample) {
            consecutiveCount++;
        } else {
            consecutiveCount = 0; // Reset counter if condition is not met
        }

        // Check if we have reached the required number of consecutive occurrences
        if (consecutiveCount >= requiredConsecutive) {
            Serial.println("Audio trend detected: Left is consistently greater than Right for 40 samples.");
            detecting = false;
            break; // Exit the loop as we have detected the pattern
        }
    }

    if (consecutiveCount < requiredConsecutive) {
        Serial.println("No significant audio trend detected.");
    }
  }
}

#define N 1024 // Number of samples

// Data arrays
float wind[N];  // Hann window
float x1[N];    // Input signal without window
float x2[N];    // Input signal with window
float y1_cf[N * 2]; // Complex array for FFT (no window)
float y2_cf[N * 2]; // Complex array for FFT (with window)

void perform_fft(float* signal, float* output) {
    memcpy(output, signal, N * sizeof(float));
    dsps_fft2r_fc32(output, N);
    dsps_bit_rev_fc32(output, N);
    dsps_cplx2reC_fc32(output, N);
}

// FFT test function
void fft_test() {
    // Generate Hann window
    dsps_wind_hann_f32(wind, N);

    // Generate input signal (for example, a combination of sine waves)
    for (int i = 0; i < N; i++) {
        x1[i] = 1.0 * sin(2 * M_PI * i / N * 50) + 0.5 * sin(2 * M_PI * i / N * 120);
    }

    // Apply the Hann window to the input signal
    for (int i = 0; i < N; i++) {
        x2[i] = x1[i] * wind[i];
    }

    // Initialize FFT
    if (dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
        Serial.println("FFT initialization failed");
        return;
    }

    // Perform FFT without window
    perform_fft(x1, y1_cf);

    // Perform FFT with Hann window
    perform_fft(x2, y2_cf);

    // Print FFT results (magnitude)
    Serial.println("FFT output without window:");
    for (int i = 0; i < N / 2; i++) {
        float real = y1_cf[i * 2];
        float imag = y1_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        Serial.printf("%d: %f\n", i, magnitude);
    }

    Serial.println("FFT output with Hann window:");
    for (int i = 0; i < N / 2; i++) {
        float real = y2_cf[i * 2];
        float imag = y2_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        Serial.printf("%d: %f\n", i, magnitude);
    }
}

void printHeapInfo() {
    Serial.println("Heap info:");
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}

void setup() {
    Serial.begin(115200);

    // Print heap info after memory allocation
    printHeapInfo();
    // Call FFT test function
    fft_test();
    // Print heap info after FFT test
    printHeapInfo();

    i2s_pin_config_t my_pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = 27,
        .ws_io_num = 26,
        .data_out_num = 25,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    
    static i2s_config_t i2s_config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 48000, // updated automatically by A2DP
      .bits_per_sample = (i2s_bits_per_sample_t)32,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = true,
      .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
    };

    a2dp_sink.set_pin_config(my_pin_config);
    a2dp_sink.set_i2s_config(i2s_config);

    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_raw_stream_reader(read_binary_data_frequency_onechannel);
    a2dp_sink.start("MyMusic");

    Serial.println("Successfully initialize I2S!");

    
}

void loop() {
}