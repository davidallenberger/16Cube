#pragma once
enum class FFTWindow { Hamming }; enum class FFTDirection { Forward };
template<class T> struct ArduinoFFT { ArduinoFFT(T*,T*,int,int){} void windowing(FFTWindow,FFTDirection){} void compute(FFTDirection){} void complexToMagnitude(){} };
