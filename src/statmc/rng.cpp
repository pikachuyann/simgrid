/* Copyright (c) 2008-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <boost/random.hpp>
#include <xbt/RngStream.h>
#include "xbt/asserts.h"
#include "src/statmc/rng.hpp"
#include <cstring>

/*
 * This is currently a work in progress; the goal is to catch every
 * probabilistic draw that may be needed for the specification of a model and
 * dispatch it to the chosen random method, whether it is the mt19937
 * Mersenne Twister or the RngStream library included with SimGrid.
 *
 * Furthermore, some functions and variables should be available to switch
 * between the two random generators and give a seed when needed.
 */

namespace simgrid {
namespace statmc {
namespace rng {
	boost::random::mt19937 mt19937_rng;
	RngStream rngstream;
	ChosenMethod currentrng = RNG_MersenneTwister;
	
	void UseMersenneTwister() { currentrng = RNG_MersenneTwister; }
	void UseRngStream() { currentrng = RNG_RngStream; }
	void UseRngStream(std::string name) {
		currentrng=RNG_RngStream;
		rngstream=RngStream_CreateStream(name.c_str());
	} // The use of c_str is required there because RngStream is a C library using char*, and will convert the C++ string to a char*.
	void SetMersenneSeed(int seed) {
		mt19937_rng.seed(seed);
	}
	
	double Exp(double lambda) {
		switch (currentrng) {
			case RNG_MersenneTwister:
				{ boost::random::exponential_distribution<> expo(lambda);
				return expo(mt19937_rng); } // A bracket is necessary because we define expo for the sole purpose of Mersenne-Twister.
			case RNG_RngStream:
				return RngStream_RandExp(rngstream,lambda);
			default:
				xbt_assert(false,"The exponential law is not implemented for the current RNG.");
		}
	}
}
}
}