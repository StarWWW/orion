/*
 * This file is part of the VanitySearch distribution (https://github.com/JeanLucPons/VanitySearch).
 * Copyright (c) 2019 Jean Luc PONS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Timer.h"

#ifdef WIN64

LARGE_INTEGER Timer::perfTickStart;
double Timer::perfTicksPerSec;
LARGE_INTEGER Timer::qwTicksPerSec;

#else

#include <unistd.h>
struct timespec Timer::tickStart;

#endif

void Timer::Init() {

#ifdef WIN64
  QueryPerformanceFrequency(&qwTicksPerSec);
  QueryPerformanceCounter(&perfTickStart);
  perfTicksPerSec = (double)qwTicksPerSec.QuadPart;
#else
  clock_gettime(CLOCK_MONOTONIC, &tickStart);
#endif
}

double Timer::get_tick() {

#ifdef WIN64
  LARGE_INTEGER t, dt;
  QueryPerformanceCounter(&t);
  dt.QuadPart = t.QuadPart - perfTickStart.QuadPart;
  return (double)(dt.QuadPart) / perfTicksPerSec;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)(ts.tv_sec - tickStart.tv_sec) + (double)(ts.tv_nsec - tickStart.tv_nsec) * 1e-9;
#endif
}

void Timer::SleepMillis(uint32_t millis) {

#ifdef WIN64
  Sleep(millis);
#else
  usleep(millis * 1000);
#endif
}
