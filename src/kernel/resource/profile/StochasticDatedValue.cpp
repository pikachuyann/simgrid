/* Copyright (c) 2004-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "src/kernel/resource/profile/StochasticDatedValue.hpp"
#include "src/statmc/rng.hpp"

namespace simgrid {
namespace kernel {
namespace profile {

double StochasticDatedValue::draw(std::string law, std::vector<double> params) {
        if (law=="DET" || law=="DETERMINISTIC") {
                return params[0];
        } else if (law=="EXP") {
                return simgrid::statmc::rng::Exp(params[0]);
        } else {
                xbt_assert(false,"Unimplemented law %s",law.c_str());
        }
}
double StochasticDatedValue::get_value() { return draw(value_law,value_params); }
double StochasticDatedValue::get_date() { return draw(date_law,date_params); }
DatedValue StochasticDatedValue::get_datedvalue() {
        DatedValue event;
        event.date_ = get_date();
        event.value_ = get_value();
        return event;
}
	
} // namespace profile
} // namespace kernel
} // namespace simgrid