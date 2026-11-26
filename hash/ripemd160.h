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

#ifndef RIPEMD160_H
#define RIPEMD160_H

#include <stdint.h>
#include <string.h>

void ripemd160_32(unsigned char *input, unsigned char *digest);

static inline bool ripemd160_comp_hash(const uint8_t *h0, const uint8_t *h1) {
  return memcmp(h0, h1, 20) == 0;
}

#endif // RIPEMD160_H
