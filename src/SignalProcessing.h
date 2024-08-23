void read_binary_data_frequency_onechannel(const uint8_t *data, uint32_t length);
void read_data_frequency_fft_onechannel(const uint8_t *data, uint32_t length);
void read_data_frequency_fft_marker(const uint8_t *data, uint32_t length);
void read_data_stream(const uint8_t *data, uint32_t length);
void decode_fsk(const uint8_t *data, uint32_t length);
void perform_fft(float* signal, float* output, uint32_t size);
void fft_test();