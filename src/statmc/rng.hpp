/* Copyright (c) 2008-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef RNG_HPP
#define RNG_HPP

#include <string>

namespace simgrid {
namespace statmc {
namespace rng {
enum ChosenMethod { RNG_MersenneTwister, RNG_RngStream };

void UseMersenneTwister();
void UseRngStream();
void UseRngStream(std::string);
void SetMersenneSeed(int);
double Exp(double);
double Unif(double, double);
} // namespace rng
} // namespace statmc
} // namespace simgrid

#endif