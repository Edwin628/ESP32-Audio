# ESP32 Bluetooth Audio

This project focuses on developing and analyzing Bluetooth audio functionalities on the ESP32 microcontroller. The project includes a variety of scripts, configurations, and samples for processing audio data, with a particular emphasis on digital signal processing (DSP) and data compression techniques.

## Table of Contents

- [Installation](#installation)
- [Project Structure](#project-structure)
- [Usage](#usage)
- [License](#license)

## Installation

To set up the project on your local machine, follow these steps:

1. Navigate to the project directory:
   ```bash
   cd ESP32-Bluetooth-Audio
   ```
2. Ensure you have [PlatformIO](https://platformio.org/) installed and configured in your environment on Vscode, refer to `platformio.ini`, the board is `Espressif ESP32 Dev Module`
- 
3. Build and upload the project to your ESP32 device using PlatformIO Upload button or :
   ```bash
   platformio run --target upload
   ```

## Project Structure
- **huffman/**: Contains Huffman coding implementation for audio data compression.
- **include/**: Header files used across the project.
- **legacy_code/**: Archive of older or deprecated code.
- **lib/**: Main libraries we are using for the project.
  - **arduinoFFT/**: Arduino FFT (Fast Fourier Transform) library for signal processing.
  - **esp-dsp/**: ESP-DSP library for digital signal processing on the ESP32 platform.
  - **README**: Documentation for libraries.
- **samples/**: Collection of generated audio samples and related data.
  - **`output.py`**: Python script for caprturing serial output data (COM5).
  - **`analyze.ipynb`**: Jupyter notebook for analyzing audio data.
  - **chirp/**: Chirp signal samples.
  - **final/**: Final processed samples.
  - **general/**: General audio samples.
  - **magnitudes/**: Magnitude data from audio processing.
  - **mobile/**: Mobile device audio samples.
  - **raw/**: Raw audio data.
  - **time/**: Time evaluation for processing.
- **thesis_validation/**: Scripts and data used for validating results, used for an academic thesis.
  - **`plot.ipynb`**: Jupyter notebook for plotting and visualizing data.
  - **`sound1-a.mp3`**: Sample audio file (used for web audio file generation).
- **CMakeLists.txt**: CMake configuration file for building the project.
- **platformio.ini**: PlatformIO configuration file.
- **src/**: The `src/` directory contains the core source files for the ESP32 Bluetooth Audio project. These files are responsible for managing Bluetooth configurations, performing FFT (Fast Fourier Transform) operations, handling signal processing tasks, and more.
  - **BluetoothConfig.cpp & BluetoothConfig.h**:
    - Implements the configuration and management of Bluetooth connectivity for the ESP32.
    - Handles pairing, device communication, and Bluetooth protocol-related tasks.
  - **FFTTest.cpp & FFTTest.h**:
    - Contains test routines and functions for performing and validating FFT operations on audio data.
    - Used to analyze the frequency domain of audio signals, crucial for DSP (Digital Signal Processing).
  - **main.cpp**:
    - The main entry point for the ESP32 firmware.
    - Initializes the system, starts Bluetooth services, and manages the overall control flow of the program.
  - **SignalProcessing.cpp & SignalProcessing.h**:
    - Implements various audio to data processing algorithms.
    - Central to manipulating and analyzing audio signals captured or transmitted via Bluetooth.
  - **Timer.cpp & Timer.h**:
    - Provides timing functions and utilities, used for intervals, timeouts, and performance measurements.
  - **Utility.h**:
    - Contains utility functions and definitions that are used across multiple parts of the project.

## Usage

1. **ESP32 Coding**:
   - Ensure your ESP32 is connected and properly set up.
   - Use PlatformIO to build and upload the firmware to the ESP32.
   - Run `./samples/output.py` when you are testing the system.
   - Monitor the ESP32's serial output for real-time data and results in `./samples/output.txt`.

2. **Analyzing Audio Data**:
   - Used for analysing `./output.txt`, usually you will changed the file name and put it under the folder `samples`
   - Use the `analyze.ipynb` Jupyter notebook to perform various analyses on audio data, such as raw output and frequency magnitude visualiztion.
   

## License

This project is used for Logitech Project: Wireless Device Interaction over Digital Audio