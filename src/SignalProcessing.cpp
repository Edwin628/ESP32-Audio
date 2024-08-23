#include <Arduino.h>
#include "esp_dsp.h"
#include <string>
#include <vector>
#include <regex>
#include "Timer.h"

#define SAMPLES 512  // Number of samples for FFT. Must be a power of 2.
#define SAMPLING_RATE 44100  // Sampling rate in Hz.
#define MAX_SAMPLES 2048
// #define MARKER
// #define TIME_M
float real[MAX_SAMPLES];   // Array to store real values for FFT.
float imag[MAX_SAMPLES];   // Array to store imaginary values for FFT.
// Data arrays for test
float wind[MAX_SAMPLES];  // Hann window
float x1[MAX_SAMPLES];    // Input signal without window
float x2[MAX_SAMPLES];    // Input signal with window
float y1_cf[MAX_SAMPLES * 2]; // Complex array for FFT (no window)
float y2_cf[MAX_SAMPLES * 2]; // Complex array for FFT (with window)

void perform_fft(float* signal, float* output, uint32_t size) {
    for (int i=0 ; i< size ; i++)
    {
        output[i*2 + 0] = signal[i]; // Real part is your signal
        output[i*2 + 1] = 0; // Imag part is 0
    }
    // memcpy(output, signal, size * sizeof(float));
    dsps_fft2r_fc32(output, size);
    dsps_bit_rev_fc32(output, size);
    dsps_cplx2reC_fc32(output, size);
}

void fft_test() {
    // Generate Hann window
    dsps_wind_hann_f32(wind, SAMPLES);

    // Generate input signal (for example, a combination of sine waves)
    for (int i = 0; i < SAMPLES; i++) {
        x1[i] = 1.0 * sin(2 * M_PI * i / SAMPLING_RATE * 1500) + 0.5 * sin(2 * M_PI * i / SAMPLING_RATE * 4000);
    }

    // Apply the Hann window to the input signal
    for (int i = 0; i < SAMPLES; i++) {
        x2[i] = x1[i] * wind[i];
    }

    // Initialize FFT
    if (dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
        printf("FFT initialization failed\n");
        return;
    }

    // Perform FFT without window
    perform_fft(x1, y1_cf, SAMPLES);

    // Perform FFT with Hann window
    perform_fft(x2, y2_cf, SAMPLES);

    // Print FFT results (magnitude) without window
    printf("FFT output without window:\n");
    for (int i = 0; i < SAMPLES / 2; i++) {
        float real = y1_cf[i * 2];
        float imag = y1_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        float frequency = i * (SAMPLING_RATE / SAMPLES);
        printf("Frequency: %f Hz, Magnitude: %f\n", frequency, magnitude);
    }

    // Print FFT results (magnitude) with Hann window
    printf("FFT output with Hann window:\n");
    for (int i = 0; i < SAMPLES / 2; i++) {
        float real = y2_cf[i * 2];
        float imag = y2_cf[i * 2 + 1];
        float magnitude = sqrt(real * real + imag * imag);
        float frequency = i * (SAMPLING_RATE / SAMPLES);
        printf("Frequency: %f Hz, Magnitude: %f\n", frequency, magnitude);
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
    uint32_t sample_count = length / 4; // Each sample is 2 bytes (mono channel)
    int consecutiveCount = 0;
    const int requiredConsecutive = 40;

    memset(real, 0, sizeof(real));
    memset(imag, 0, sizeof(imag));

    for (uint32_t i = 0; i < sample_count; i++) {
        int16_t left_sample = samples[2 * i];
        int16_t right_sample = samples[2 * i + 1];
        Serial.print("Sample ");
        Serial.print(i);
        Serial.print(": ");
        // Serial.println(left_sample);
        real[i] = (float)left_sample;
        Serial.println(real[i]);
    }

    // Determine the next power of 2 greater than sample_count
    uint32_t fft_size = 1;
    while (fft_size < sample_count) {
        fft_size <<= 1;
    }
    fft_size = (fft_size > MAX_SAMPLES) ? MAX_SAMPLES : fft_size;
    // fft_size = fft_size >> 1;
    Serial.print("fft size: ");
    Serial.println(fft_size);

    // Initialize FFT
    if (dsps_fft2r_init_fc32(NULL, fft_size) != ESP_OK) {
        Serial.println("FFT initialization failed");
        return;
    }

    // Generate Hann window
    dsps_wind_hann_f32(wind, fft_size);

    // Apply Hann window to the input signal
    for (uint32_t i = 0; i < sample_count; i++) {
        x2[i] = real[i] * wind[i];
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
        Serial.print("magnitude1: ");
        Serial.println(magnitude1);
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
    Serial.print("Detected frequency without window: ");
    Serial.println(frequency1);

    
    float frequency2 = (float)max_index2 * SAMPLING_RATE / fft_size;
    Serial.print("Detected frequency with window: ");
    Serial.println(frequency2);
    

    // Decode frequency to message
    decoded_message[0] = decode_ascii_frequency(frequency1);  // Use windowed FFT result for decoding
    decoded_message[1] = '\0';  // Null-terminate the string

    Serial.print("Decoded message: ");
    Serial.println(decoded_message);
}

enum State {
    IDLE,
    SYNC,
    PREAMBLE,
    DATA,
    END
};

State currentState = IDLE;
int consecutiveHighCount = 0;
int consecutiveLowCount = 0;
const int requiredHighCount = 200; 
const int requiredLowCount = 20;  
const int thresholdDetect = 25000;      
const int thresholdZero = 10000;
int consecutiveZeroCount = 0;
int sampleCount = 480;
const int requiredZeroCount = 20;
bool fftInitialized = false;
uint32_t currentFftSize = 0;
std::vector<char> decoded_chars;
Timer timer;


const std::vector<int> commonSamplingRates = {
    80, 160, 220, 240, 320,
    441, 480, 882, 960, 1920
};



void initializeFFT(uint32_t fft_size) {
    if (!fftInitialized || fft_size != currentFftSize) {
        if (dsps_fft2r_init_fc32(NULL, fft_size) != ESP_OK) {
            Serial.println("FFT initialization failed");
            return;
        }
        fftInitialized = true;
        currentFftSize = fft_size;
        dsps_wind_hann_f32(wind, fft_size);
        Serial.println("FFT initialized");
        #ifdef TIME_M
        timer.reset();
        #endif
    }
}

void detect_start_marker(int16_t left_sample, int16_t right_sample, int& consecutiveHighCount, int requiredHighCount) {
    if (left_sample > right_sample && abs(left_sample) > thresholdDetect && abs(right_sample) > thresholdDetect) {
        consecutiveHighCount++;
        if (consecutiveHighCount >= requiredHighCount) {
            detecting = false;
            currentState = SYNC;
            consecutiveHighCount = 0;
            #ifdef MARKER
            Serial.println("Start marker detected");
            #endif
        }
    } else {
        consecutiveHighCount = 0;
    }
}

bool detect_end_marker(int16_t left_sample, int16_t right_sample, int& consecutiveLowCount, int requiredLowCount) {
    if (left_sample < right_sample && abs(left_sample) > thresholdDetect && abs(right_sample) > thresholdDetect) {
        consecutiveLowCount++;
        if (consecutiveLowCount >= requiredLowCount) {
            consecutiveLowCount = 0;
            return true;
        }
    } else {
        consecutiveLowCount = 0;
    }
    return false;
}


char decode_ascii_from_frequencies(float* magnitudes, uint32_t fft_size, float sampling_rate) {
    const uint16_t frequencies[] = {5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000};
    // const uint16_t frequencies[] = {20000, 21000, 22000, 23000, 24000, 25000, 26000, 27000};
    // const int num_frequencies = sizeof(frequencies) / sizeof(frequencies[0]);
    const int num_frequencies = 8;
    char decoded_char = 0;
    
    // for (int i = 0; i < num_frequencies; i++) {
    //     int index = (int)(frequencies[i] * fft_size / sampling_rate);
    //     if (magnitudes[index] > 120) {
    //         decoded_char |= (1 << i);
    //     }
    // }

    for (int i = 0; i < num_frequencies; i++) {
        int index = (int)(frequencies[i] * fft_size / sampling_rate);
        if (index < (fft_size / 2 + 1)) {
            int lower_index = index > 0 ? index - 1 : index;
            int upper_index = index < (fft_size / 2) ? index + 1 : index;
            if (magnitudes[index] > 120 || magnitudes[lower_index] > 120 || magnitudes[upper_index] > 120) {
                decoded_char |= (1 << i);
            }
        }
    }
    unsigned char mask = ~(1 << 7);
    decoded_char = decoded_char & mask;

    return decoded_char;
}

char decode_ascii_from_frequencies_chirp(float* magnitudes, uint32_t fft_size, float sampling_rate) {
    const uint16_t frequencies[] = {5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000};
    const int num_frequencies = 8;
    char decoded_char = 0;
    const float threshold = 40.0;
    const int search_range = 8; 
    const int required_count = 5; 

    for (int i = 0; i < num_frequencies; i++) {
        int index = (int)(frequencies[i] * fft_size / sampling_rate);
        if (index < (fft_size / 2 + 1)) {
            int count = 0;
            for (int j = -search_range; j <= search_range; j++) {
                int check_index = index + j;
                if (check_index >= 0 && check_index <= (fft_size / 2) && magnitudes[check_index] > threshold) {
                    count++;
                }
            }
            if (count >= required_count) {
                decoded_char |= (1 << i);
            }
        }
    }

    decoded_char &= ~(1 << 7);

    return decoded_char;
}

int findClosestSamplingRate(int sampleCount) {
    auto closestRate = *std::min_element(commonSamplingRates.begin(), commonSamplingRates.end(),
        [sampleCount](int a, int b) {
            return std::abs(a - sampleCount) < std::abs(b - sampleCount);
        });

    return closestRate;
}

void process_fft_and_decode(float* input, uint32_t fft_size, float sampling_rate) {
    float magnitudes[fft_size / 2];

    perform_fft(input, y1_cf, fft_size);

    for (int i = 0; i < fft_size / 2; i++) {
        float real = y1_cf[i * 2];
        float imag = y1_cf[i * 2 + 1];
        magnitudes[i] = sqrt(real * real + imag * imag);
    }

    #ifdef MARKER
    Serial.println("Magnitudes:");
    for (int i = 0; i < fft_size / 2; i++) {
        Serial.print(magnitudes[i]);
        Serial.print(" ");
    }
    Serial.println();
    #endif

    char decoded_char = decode_ascii_from_frequencies_chirp(magnitudes, fft_size, sampling_rate);
    #ifdef MARKER
    Serial.print("Decoded char: ");
    Serial.println(decoded_char);
    #endif

    decoded_chars.push_back(decoded_char);

}


void process_sample(int16_t left_sample, int16_t right_sample, int& consecutiveHighCount, int& consecutiveLowCount, uint32_t fft_size) {
    static float fft_buffer[500];
    static int buffer_index = 0;

    switch (currentState) {
        case IDLE:
            detect_start_marker(left_sample, right_sample, consecutiveHighCount, requiredHighCount);
            break;
        
        case SYNC:
            #ifdef TIME_M
            timer.start();
            #endif
            if (abs(left_sample) < thresholdZero && abs(right_sample) < thresholdZero) {
                consecutiveZeroCount++;
            } else {
                if (consecutiveZeroCount >= requiredZeroCount && (abs(left_sample) > thresholdZero || abs(right_sample) > thresholdZero)) {
                    currentState = DATA;
                    sampleCount = consecutiveZeroCount*5;
                    sampleCount = findClosestSamplingRate(sampleCount);
                    #ifdef MARKER
                    Serial.print("sampleCount: ");
                    Serial.println(sampleCount);
                    Serial.println("SYNC detected, starting data reception");
                    #endif
                }
                consecutiveZeroCount = 0;
            }
            break;

        case PREAMBLE:
            #ifdef MARKER
            // Serial.println("State Preamble");
            // Serial.println(consecutiveZeroCount);
            #endif
            if (abs(left_sample) < thresholdZero && abs(right_sample) < thresholdZero) {
                consecutiveZeroCount++;
            } else {
                if (consecutiveZeroCount >= requiredZeroCount && (abs(left_sample) > thresholdZero || abs(right_sample) > thresholdZero)) {
                    currentState = DATA;
                    // Serial.print("sampleCount: ");
                    // Serial.println(sampleCount);
                    #ifdef MARKER
                    Serial.println("Preamble detected, continue data reception");
                    #endif
                }
                consecutiveZeroCount = 0;
            }
            break;

        case DATA:
            fft_buffer[buffer_index++] =  static_cast<float>(left_sample) / 32768.0f;
            // fft_buffer[buffer_index] = fft_buffer[buffer_index] * wind[buffer_index];
            // Serial.print("fft buffer index: ");
            // Serial.println(buffer_index);

            if (buffer_index >= sampleCount) {
                // Serial.println("Process FFT and decode now");
                // Serial.print("FFT size: ");
                // Serial.println(fft_size);
                process_fft_and_decode(fft_buffer, fft_size, sampleCount*100);
                memset(fft_buffer, 0, sizeof(fft_buffer));
                buffer_index = 0;
                currentState = PREAMBLE;
            }

            // end marker
            if (detect_end_marker(left_sample, right_sample, consecutiveLowCount, requiredLowCount)) {
                memset(fft_buffer, 0, sizeof(fft_buffer));
                buffer_index = 0;
                currentState = END;
                #ifdef MARKER
                Serial.println("End marker detected, stopping data reception.");
                #endif
            }
            break;

        case END:
            #ifdef TIME_M
            timer.stop();
            Serial.printf("Total time spent in read_data_frequency_fft_marker: %lld ms\n", timer.getTotalDuration());
            timer.reset();
            #endif
            for (char c: decoded_chars) {
                Serial.print(c);
            }
            Serial.println();
            #ifdef MARKER
            Serial.println();
            Serial.println("End printing finished, idle state.");
            #endif
            decoded_chars.clear();
            currentState = IDLE;
            break;
    }
}

void read_data_frequency_fft_marker(const uint8_t *data, uint32_t length) {
    int16_t *samples = (int16_t*) data;
    uint32_t sample_count = length / 4; // Each sample is 2 bytes (mono channel)
    int consecutiveCount = 0;
    const int requiredConsecutive = 40;

    memset(real, 0, sizeof(real));
    memset(imag, 0, sizeof(imag));

    // Determine the next power of 2 greater than sample_count
    uint32_t fft_size = 1;
    while (fft_size < sample_count) {
        fft_size <<= 1;
    }
    fft_size = (fft_size > MAX_SAMPLES) ? MAX_SAMPLES : fft_size;
    // fft_size = fft_size >> 1;
    // Serial.print("fft size: ");
    // Serial.println(fft_size);

    initializeFFT(fft_size);

    for (uint32_t i = 0; i < sample_count; i++) {
        int16_t left_sample = samples[2 * i];
        int16_t right_sample = samples[2 * i + 1];
        // Serial.print("Receving sample ");
        // Serial.print(i);
        // Serial.print(": ");
        // Serial.print(left_sample);
        // Serial.print(", ");
        // Serial.println(right_sample);
        real[i] = (float)left_sample;
        process_sample(left_sample, right_sample, consecutiveHighCount, consecutiveLowCount, fft_size);
    }
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
