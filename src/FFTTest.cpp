#include <Arduino.h>
#include "esp_dsp.h"
#include "SignalProcessing.h"
#include <string>

uint32_t next_power_of_2(uint32_t n) {
    uint32_t power = 1;
    while (power < n) {
        power <<= 1;
    }
    return power;
}

uint32_t total_size = 3000;  // 总信号大小
uint32_t chunk_size = 1024;  // 块大小
float sampling_rate_test = 1000.0;  // 采样率，单位为Hz
float signal_test[3000];  // 示例信号

void test_frequency_spectrum() {
    // 生成一个示例信号（例如，50 Hz和120 Hz的正弦波）
    for (uint32_t i = 0; i < total_size; i++) {
        signal_test[i] = sin(2 * M_PI * 50 * i / sampling_rate_test) + sin(2 * M_PI * 120 * i / sampling_rate_test);
    }

    uint32_t padded_size = next_power_of_2(chunk_size);
    float output[padded_size];
    float aggregated_magnitudes[padded_size / 2];
    memset(aggregated_magnitudes, 0, sizeof(aggregated_magnitudes));

    uint32_t overlap = chunk_size / 2;  // 50%重叠
    uint32_t step = chunk_size - overlap;

    for (uint32_t start = 0; start < total_size; start += step) {
        // 处理每个块
        uint32_t size = (start + chunk_size <= total_size) ? chunk_size : (total_size - start);
        float chunk[padded_size];
        memset(chunk, 0, sizeof(chunk));  // 零填充
        memcpy(chunk, signal_test + start, size * sizeof(float));

        // 应用窗口函数
        dsps_wind_hann_f32(chunk, size);

        perform_fft(chunk, output, padded_size);

        for (uint32_t i = 0; i < padded_size / 2; i++) {
            float real = output[2 * i];
            float imag = output[2 * i + 1];
            float magnitude = sqrt(real * real + imag * imag);
            aggregated_magnitudes[i] += magnitude;
        }
    }

    // 计算平均幅值
    uint32_t num_blocks = total_size / step;
    for (uint32_t i = 0; i < padded_size / 2; i++) {
        aggregated_magnitudes[i] /= num_blocks;
    }

    // 计算频率
    float frequencies[padded_size / 2];
    for (uint32_t i = 0; i < padded_size / 2; i++) {
        frequencies[i] = i * sampling_rate_test / padded_size;
    }

    // 打印频率和平均幅值
    for (uint32_t i = 0; i < padded_size / 2; i++) {
        Serial.printf("Frequency: %f Hz, Average Magnitude: %f\n", frequencies[i], aggregated_magnitudes[i]);
    }
}

#define SAMPLES_TEST 512  // Number of samples for FFT. Must be a power of 2.
#define SAMPLING_RATE_TEST 512  // Sampling rate in Hz.

float windTest[SAMPLES_TEST];  // Hann window
float x1Test[SAMPLES_TEST];    // Input signal without window
float x2Test[SAMPLES_TEST];    // Input signal with window
float y1_cf_test[SAMPLES_TEST * 2]; // Complex array for FFT (no window)
float y2_cf_test[SAMPLES_TEST * 2]; // Complex array for FFT (with window)

