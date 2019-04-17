/* Copyright (c) 2008-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/s4u/Engine.hpp"
#include "simgrid/simix.h"
#include "src/simix/smx_private.hpp"
#include <ostream>

namespace simgrid {
namespace statmc {

void multirun(int nbruns)
{
  double simulationEndTime = 0.0;
  for (int i = 0; i < nbruns; i++) {
    if (i > 0) {
      SIMIX_reinit();
    }
    SIMIX_run();
    simulationEndTime = SIMIX_get_clock();
    printf("(Debug): Ending time (for run %i) was %f.\n", i, simulationEndTime);
  }
}

void multirun(int nbruns, const std::string& deploy)
{
  double simulationEndTime = 0.0;
  for (int i = 0; i < nbruns; i++) {
    if (i > 0) {
      SIMIX_reinit();
    }
    SIMIX_launch_application(deploy);
    SIMIX_run();
    simulationEndTime = SIMIX_get_clock();
    printf("(Debug): Ending time (for run %i) was %f.\n", i, simulationEndTime);
  }
}

} // namespace statmc
} // namespace simgrid
