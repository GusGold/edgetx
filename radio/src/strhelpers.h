/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _STRHELPERS_H_
#define _STRHELPERS_H_

#include "definitions.h"
#include "opentx_types.h"
#include <string>

#include <string>
#include <cstring>

#define SHOW_TIME  0x1
#define SHOW_TIMER 0x0
#define SHOW_TIMER_UPPER_CASE   0x2
#define SHOW_TIMER_HM_FORMAT    0x5

PACK(typedef struct {
  uint8_t showTime:1;
  uint8_t upperCase:1;
  uint8_t numDigitGroups:3;  
  uint8_t hmFormat:1;
  uint8_t reserved:2;
}) TimerDisplayOptions;

typedef union  {
  uint8_t options;
  TimerDisplayOptions displayOptions;
} TimerOptions;

char hex2zchar(uint8_t hex);
char hex2char(uint8_t hex);
char zchar2char(int8_t idx);
char char2lower(char c);
int8_t char2zchar(char c);
void str2zchar(char *dest, const char *src, int size);
int zchar2str(char *dest, const char *src, int size);
int strnlen(const char *src, int max_size);
unsigned int effectiveLen(const char * str, unsigned int size);

char *strAppend(char *dest, const char *source, int len = 0);
char *strAppendUnsigned(char *dest, uint32_t value, uint8_t digits = 0,
                        uint8_t radix = 10);
char *strAppendSigned(char *dest, int32_t value, uint8_t digits = 0,
                      uint8_t radix = 10);
char *strSetCursor(char *dest, int position);
char *strAppendDate(char *str, bool time = false);
char *strAppendFilename(char *dest, const char *filename, const int size);
std::string formatNumberAsString(int32_t val, LcdFlags flags = 0, uint8_t len = 0, const char * prefix = nullptr, const char * suffix = nullptr);

#if !defined(BOOT)
char *getStringAtIndex(char *dest, const char *const *s, int idx);
char *strAppendStringWithIndex(char *dest, const char *s, int idx);
#define LEN_TIMER_STRING 10  // "-00:00:00"
char *getTimerString(char *dest, int32_t tme, TimerOptions timerOptions = {.options = 0});
char *getFormattedTimerString(char *dest, int32_t tme, TimerOptions timerOptions);
char *getCurveString(char *dest, int idx);
char *getGVarString(char *dest, int idx);
char *getGVarString(int idx);
char *getValueOrGVarString(char *dest, size_t len, gvar_t value, gvar_t vmin,
                           gvar_t vmax, LcdFlags flags = 0,
                           const char *suffix = nullptr, gvar_t offset = 0);
const char *getSwitchWarnSymbol(uint8_t pos);
const char *getSwitchPositionSymbol(uint8_t pos);
char *getSwitchPositionName(char *dest, swsrc_t idx);
char *getSwitchName(char *dest, swsrc_t idx);

template<size_t L>
char* getSourceString(char (&dest)[L], mixsrc_t idx);

template <size_t L>
char *getSourceCustomValueString(char (&dest)[L], source_t source, int32_t val,
                                 LcdFlags flags);

int  getRawSwitchIdx(char sw);
char getRawSwitchFromIdx(int sw);
#endif

char *getFlightModeString(char *dest, int8_t idx);

char *getSourceString(mixsrc_t idx);
char *getSourceCustomValueString(source_t source, int32_t val, LcdFlags flags);
char *getSwitchPositionName(swsrc_t idx);
char *getCurveString(int idx);
char *getTimerString(int32_t tme, TimerOptions timerOptions = {.options = 0});
void splitTimer(char *s0, char *s1, char *s2, char *s3, int tme,
                bool bLowercase = true);

template<size_t N>
std::string stringFromNtString(const char (&a)[N]) {
    return std::string(a, strnlen(a, N));        
}    
template<size_t L>
void copyToUnTerminated(char (&dest)[L], const char* const src) {
    strncpy(dest, src, L);
}
template<size_t L>
void copyToUnTerminated(char (&dest)[L], const std::string& src) {
    strncpy(dest, src.c_str(), L);
}
template<typename S>
void clearStruct(S& s) {
    memset((void*) &s, 0, sizeof(S));
}

template <size_t N>
using offset_t = std::integral_constant<size_t, N>;

template <size_t DL, size_t SL, size_t O = 0>
void copyToTerminated(char (&dest)[DL], const char (&src)[SL],
                      const offset_t<O> = offset_t<0>{})
{
  // unfortinately std::min() isn't constexpr in C++11
  // static constexpr size_t len = std::min(DL - O - 1, SL);
  static constexpr size_t dl{DL - O - 1};
  static_assert(dl > 0, "wrong sizes or offset");
  static constexpr size_t len = (dl < SL) ? dl : SL;
  strncpy(&dest[O], &src[0], len);
  static_assert((len + O) < DL, "wrong sizes of offset");
  dest[len + O] = '\0';
}

template <size_t L1, size_t L2>
int strncasecmp(char (&s1)[L1], const char (&s2)[L2])
{
  static constexpr size_t len = (L1 < L2) ? L1 : L2;
  return strncasecmp(s1, s2, len);
}

template <size_t L1>
int strncasecmp(char (&s1)[L1], const char *const s2)
{
  return strncasecmp(s1, s2, L1);
}

#endif  // _STRHELPERS_H_
