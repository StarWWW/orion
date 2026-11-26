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

#include "Point.h"

Point::Point() {
}

Point::Point(const Point &p) {
  x.Set((Int *)&p.x);
  y.Set((Int *)&p.y);
  z.Set((Int *)&p.z);
}

void Point::Clear() {
  x.SetInt32(0);
  y.SetInt32(0);
  z.SetInt32(0);
}

Point::~Point() {
}

bool Point::isZero() {
  return x.IsZero() && y.IsZero();
}

void Point::Reduce() {

  Int i(&z);
  i.ModInv();
  x.ModMul(&x,&i);
  y.ModMul(&y,&i);
  z.SetInt32(1);

}
