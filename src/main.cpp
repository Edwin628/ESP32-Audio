#include <Arduino.h>
#include "BluetoothConfig.h"
#include "SignalProcessing.h"
#include "Utility.h"

void setup() {
    Serial.begin(115200);
    
    // Print heap info after memory allocation
    // printHeapInfo();
    // Call FFT test function
    // fft_test();
    // Print heap info after FFT test
    // printHeapInfo();
    // test_frequency_spectrum();
    
    configureBluetooth();
    Serial.println("Successfully initialized I2S!");
}

void loop() {
    // Main loop logic if needed
}
