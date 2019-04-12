/* Copyright (c) 2004-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef STOCHASTICDATEDVALUE_HPP
#define STOCHASTICDATEDVALUE_HPP
#include "src/kernel/resource/profile/DatedValue.hpp"
#include <vector>

namespace simgrid {
namespace kernel {
namespace profile {

class DatedValue;
class StochasticDatedValue {
public:
  std::string date_law;
  std::vector<double> date_params;
  std::string value_law;
  std::vector<double> value_params;
  DatedValue get_datedvalue();
  double get_date();
  double get_value();
  explicit StochasticDatedValue() = default;
  explicit StochasticDatedValue(double d, double v)
      : date_law("DET"), date_params({d}), value_law("DET"), value_params({v})
  {
  }

private:
  double draw(std::string law, std::vector<double> params);
};

} // namespace profile
} // namespace kernel
} // namespace simgrid

#endif