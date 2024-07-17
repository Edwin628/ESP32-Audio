#include <Arduino.h>
#include "esp_dsp.h"
#include <string>
#include <vector>
#include <regex>

#define SAMPLES 512  // Number of samples for FFT. Must be a power of 2.
#define SAMPLING_RATE 48000  // Sampling rate in Hz.



void perform_fft(float* signal, float* output, uint32_t size) {
    memcpy(output, signal, size * sizeof(float));
    dsps_fft2r_fc32(output, size);
    dsps_bit_rev_fc32(output, size);
    dsps_cplx2reC_fc32(output, size);
}

#define MAX_SAMPLES 2048
float real[MAX_SAMPLES];   // Array to store real values for FFT.
float imag[MAX_SAMPLES];   // Array to store imaginary values for FFT.
// Data arrays for test
float wind[MAX_SAMPLES];  // Hann window
float x1[MAX_SAMPLES];    // Input signal without window
float x2[MAX_SAMPLES];    // Input signal with window
float y1_cf[MAX_SAMPLES * 2]; // Complex array for FFT (no window)
float y2_cf[MAX_SAMPLES * 2]; // Complex array for FFT (with window)

void fft_test() {
    // Generate Hann window
    dsps_wind_hann_f32(wind, SAMPLES);

    // Generate input signal (for example, a combination of sine waves)
    for (int i = 0; i < SAMPLES; i++) {
        x1[i] = 1.0 * sin(2 * M_PI * i / SAMPLES * 50) + 0.5 * sin(2 * M_PI * i / SAMPLES * 120);
    }

    // Apply the Hann window to the input signal
    for (int i = 0; i < SAMPLES; i++) {
        x2[i] = x1[i] * wind[i];
    }

    // Initialize FFT
    if (dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
        Serial.println("FFT initialization failed");
        return;
    }

    // Perform FFT without window
    perform_fft(x1, y1_cf, SAMPLES);

    // Perform FFT with Hann window
    perform_fft(x2, y2_cf, SAMPLES);

    // Print FFT results (magnitude)
    Serial.println("FFT output without window:");
    for (int i = 0; i < SAMPLES / 2; i++) {
        float real = y1_cf[i * 2];
        float imag = y1_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        Serial.printf("%d: %f\n", i, magnitude);
    }

    Serial.println("FFT output with Hann window:");
    for (int i = 0; i < SAMPLES / 2; i++) {
        float real = y2_cf[i * 2];
        float imag = y2_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        Serial.printf("%d: %f\n", i, magnitude);
    }
}


bool detecting = true;
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

char decode_ascii_frequency(float frequency) {
    int asciiCode = static_cast<int>(frequency / frequencyBase);
    if (asciiCode >= 32 && asciiCode <= 126) {
        return static_cast<char>(asciiCode);
    } else {
        return '?'; // Unknown frequency
    }
}

std::vector<std::string> extract_data(const std::string& array, int chunk_size) {
    std::string cleaned_array;
    for (char c : array) {
        if (c != '?') {
            cleaned_array += c;
        }
    }
    std::regex pattern("1+0*|0+1*");
    std::sregex_iterator iter(cleaned_array.begin(), cleaned_array.end(), pattern);
    std::sregex_iterator end;

    if (iter == end) {
        throw std::invalid_argument("No valid pattern found in the array");
    }

    int interval = 2400 / chunk_size;

    std::vector<std::string> result;
    for (size_t i = 0; i < cleaned_array.length(); i += interval) {
        std::string segment = cleaned_array.substr(i, interval);
        if (!segment.empty()) {
            result.push_back(segment);
        }
    }

    return result;
}

void check_ratio(uint32_t &sample_count_for_current_freq, int &previous_sample_count) {
    int sample_growth = sample_count_for_current_freq - previous_sample_count;
    previous_sample_count = sample_count_for_current_freq;

    if (sample_growth != 0) {
        Serial.print("Sample growth: ");
        Serial.println(sample_growth);
    }
}

void detect_start(int16_t left_sample, int16_t right_sample, int &consecutiveCount, const int requiredConsecutive) {
    if (left_sample > right_sample) {
        consecutiveCount++;
    } else {
        consecutiveCount = 0;
    }

    if (consecutiveCount >= requiredConsecutive) {
        Serial.println("Audio trend detected: Left is consistently greater than Right for 40 samples.");
        detecting = false;
    }
}

void read_binary_data_frequency_onechannel(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 4;

    int zero_crossings = 0;
    uint32_t sample_count_for_current_freq = 0;
    int consecutiveCount = 0;
    const int requiredConsecutive = 40;
    int previous_sample_count = 0;
    int previous_zero_crossings = 0;

    Serial.println("Stream received:");
    for (uint32_t i = 0; i < sample_count; i++) {
        int16_t left_sample = samples[2 * i];
        int16_t right_sample = samples[2 * i + 1];

#ifdef DEBUG
        Serial.print("Sample ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(left_sample);
        Serial.print("\t");
        Serial.println(right_sample);
#endif

        if (detecting) {
            detect_start(left_sample, right_sample, consecutiveCount, requiredConsecutive);
        } else {
            if (!is_sine_wave && abs(left_sample) > threshold) {
                is_sine_wave = true;
                cumulative_sample_count++;
                sample_count_for_current_freq++;
                previous_sample = left_sample;
                continue;
            }

            if (is_sine_wave) {
                cumulative_sample_count++;
                sample_count_for_current_freq++;

                if ((previous_sample >= 0 && left_sample < 0) || (previous_sample < 0 && left_sample >= 0)) {
                    zero_crossings++;
                    check_ratio(sample_count_for_current_freq, previous_sample_count);
                }

                previous_sample = left_sample;

                if (abs(left_sample) <= threshold) {
                    low_threshold_count++;
                    if (low_threshold_count >= continuous_low_threshold_samples) {
                        is_sine_wave = false;
                        Serial.println("Ending flags detected");
                        Serial.print("Final Decoded message: ");
                        Serial.println(decoded_message);
                        reset_detection();
                    }
                } else {
                    low_threshold_count = 0;
                }
            }
        }
    }

    if (is_sine_wave && sample_count_for_current_freq > 0) {
        float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count_for_current_freq);
        if (frequency_count < MAX_FREQUENCIES) {
            frequencies[frequency_count++] = estimated_frequency;
        }
        Serial.print("Detected frequency: ");
        Serial.println(estimated_frequency);
    }

    for (uint8_t i = 0; i < frequency_count; i++) {
        decoded_message[i] = decode_ascii_frequency(frequencies[i]);
    }
    decoded_message[frequency_count] = '\0';

    Serial.print("Decoded message: ");
    Serial.println(decoded_message);
}

void read_data_frequency_fft_onechannel(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2; // Each sample is 2 bytes (mono channel)

    // Determine the next power of 2 greater than sample_count
    uint32_t fft_size = 1;
    while (fft_size < sample_count) {
        fft_size <<= 1;
    }
    fft_size = (fft_size > MAX_SAMPLES) ? MAX_SAMPLES : fft_size;

    for (uint32_t i = 0; i < sample_count; i++) {
        real[i] = (float)samples[i];
        imag[i] = 0.0;
    }

    // Zero padding if necessary
    for (uint32_t i = sample_count; i < fft_size; i++) {
        real[i] = 0.0;
        imag[i] = 0.0;
    }

    // Generate Hann window
    dsps_wind_hann_f32(wind, fft_size);

    // Apply Hann window to the input signal
    for (uint32_t i = 0; i < sample_count; i++) {
        x2[i] = real[i] * wind[i];
    }
    for (uint32_t i = sample_count; i < fft_size; i++) {
        x2[i] = 0.0; // Ensure the rest is zero-padded
    }

    // Initialize FFT
    if (dsps_fft2r_init_fc32(NULL, fft_size) != ESP_OK) {
        Serial.println("FFT initialization failed");
        return;
    }

    // Perform FFT without window
    perform_fft(real, y1_cf, fft_size);

    // Perform FFT with Hann window
    perform_fft(x2, y2_cf, fft_size);

    // Calculate magnitude and find the peak frequency for both cases
    float max_mag1 = 0.0, max_mag2 = 0.0;
    int max_index1 = 0, max_index2 = 0;

    for (int i = 0; i < fft_size / 2; i++) {
        // Without window
        float real1 = y1_cf[i * 2];
        float imag1 = y1_cf[i * 2 + 1];
        float magnitude1 = sqrt(real1 * real1 + imag1 * imag1);
        if (magnitude1 > max_mag1) {
            max_mag1 = magnitude1;
            max_index1 = i;
        }

        // With window
        float real2 = y2_cf[i * 2];
        float imag2 = y2_cf[i * 2 + 1];
        float magnitude2 = sqrt(real2 * real2 + imag2 * imag2);
        if (magnitude2 > max_mag2) {
            max_mag2 = magnitude2;
            max_index2 = i;
        }
    }

    // Calculate frequency
    float frequency1 = (float)max_index1 * SAMPLING_RATE / fft_size;
    float frequency2 = (float)max_index2 * SAMPLING_RATE / fft_size;

    Serial.print("Detected frequency without window: ");
    Serial.println(frequency1);

    Serial.print("Detected frequency with window: ");
    Serial.println(frequency2);

    // Decode frequency to message
    decoded_message[0] = decode_ascii_frequency(frequency2);  // Use windowed FFT result for decoding
    decoded_message[1] = '\0';  // Null-terminate the string

    Serial.print("Decoded message: ");
    Serial.println(decoded_message);
}


void read_data_stream(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2;

    const float sampling_rate = 48000;

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
    float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count);
    Serial.print("Estimated frequency: ");
    Serial.println(estimated_frequency);
}

void decode_fsk(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 2;

    const float threshold_frequency = 1000.0;
    const float sampling_rate = 8000.0;

    Serial.println("Decoding FSK stream:");
    
    int zero_crossings = 0;
    for (uint32_t i = 1; i < sample_count; i++) {
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            zero_crossings++;
        }
    }

    float estimated_frequency = (zero_crossings * sampling_rate) / (2 * sample_count);

    String decoded_data = "";
    if (estimated_frequency > threshold_frequency) {
        decoded_data = "1";
    } else {
        decoded_data = "0";
    }

    Serial.print("Estimated frequency: ");
    Serial.println(estimated_frequency);
}
