#pragma once

// Numerical host test double, not the ESP32 FFT library. A deliberately simple
// reference DFT exercises the production acquisition and band-mapping code with
// known waveforms. Firmware must continue using its real ArduinoFFT dependency.
#include <cmath>
#include <vector>

enum class FFTWindow { Hamming };
enum class FFTDirection { Forward };

template<class T> struct ArduinoFFT {
    T *real;
    T *imaginary;
    int size;

    ArduinoFFT ( T *r, T *i, int count, int ) : real ( r ), imaginary ( i ), size ( count ) {}

    void windowing ( FFTWindow, FFTDirection ) {
        for ( int i = 0; i < size; ++i ) {
            real[i] *= 0.54 - 0.46 * std::cos ( 6.283185307179586 * i / ( size - 1 ) );
        }
    }

    void compute ( FFTDirection ) {
        std::vector<T> input ( real, real + size );

        for ( int k = 0; k < size; ++k ) {
            real[k] = 0;
            imaginary[k] = 0;

            for ( int n = 0; n < size; ++n ) {
                const double phase = 6.283185307179586 * k * n / size;
                real[k] += input[n] * std::cos ( phase );
                imaginary[k] -= input[n] * std::sin ( phase );
            }
        }
    }

    void complexToMagnitude () {
        for ( int i = 0; i < size; ++i ) {
            real[i] = std::hypot ( real[i], imaginary[i] );
        }
    }
};
