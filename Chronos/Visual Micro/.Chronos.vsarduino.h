//Board = Arduino Uno
#define ARDUINO 101
#define __AVR_ATmega328P__
#define F_CPU 16000000L
#define __AVR__
#define __cplusplus
#define __attribute__(x)
#define __inline__
#define __asm__(x)
#define __extension__
#define __ATTR_PURE__
#define __ATTR_CONST__
#define __inline__
#define __asm__ 
#define __volatile__
#define __builtin_va_list
#define __builtin_va_start
#define __builtin_va_end
#define __DOXYGEN__
#define prog_void
#define PGM_VOID_P int
#define NOINLINE __attribute__((noinline))

typedef unsigned char byte;
extern "C" void __cxa_pure_virtual() {}

//already defined in arduno.h
//already defined in arduno.h
void NextMode(int nextmode);
void LEDDrawArray(char bitmap[], byte offsetrow, byte offsetcol);
boolean IsNight();
void SayTemperature();
void SayTime();
void ShowTime();
void ShowTemperature();
void loadFont(byte fontindex);
void loadSentence(byte sentenceindex);
void EEPROMClearMemory();
void EEPROMWriteLong(int p_address, long p_value);
long EEPROMReadLong(int p_address);

#include "C:\Personal\arduino-1.0.1\hardware\arduino\variants\standard\pins_arduino.h" 
#include "C:\Personal\arduino-1.0.1\hardware\arduino\cores\arduino\arduino.h"
#include "C:\Users\graham.richter\Dropbox\Graham\Projects\Arduino\Chronos\Chronos\Chronos.ino"
