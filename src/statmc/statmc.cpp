/* Copyright (c) 2008-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "src/statmc/statmc.hpp"
#include "simgrid/Exception.hpp"
#include "simgrid/s4u/Engine.hpp"
#include "simgrid/simix.h"
#include "src/simix/smx_private.hpp"
#include <ostream>

XBT_LOG_NEW_CATEGORY(statmc, "Log channels of the statistical model-checking module");
XBT_LOG_NEW_DEFAULT_SUBCATEGORY(statmc_main, statmc, "Logging specific to the statistical model-checking module");

namespace simgrid {
namespace statmc {

void multirun(int nbruns)
{
  XBT_CCRITICAL(statmc, "It is not currently possible to multi-run a simulation without reloading the deployment file "
                        "at each simulation. Please provide a deployment file while using multirun.");
  /*  double simulationEndTime = 0.0;
    for (int i = 0; i < nbruns; i++) {
      if (i > 0) {
        SIMIX_reinit();
      }
      SIMIX_run();
      simulationEndTime = SIMIX_get_clock();
      XBT_CINFO(statmc, "Ending time (for run %i) was %f.", i, simulationEndTime);
    } */
}

void multirun(int nbruns, const std::string& deploy)
{
  multirun(nbruns, deploy, 2000000.0);
}

void multirun(int nbruns, const std::string& deploy, double singlesimto)
{
  SIMIX_set_simulationtimeout(singlesimto);
  double simulationEndTime = 0.0;
  double averageEndTime    = 0.0;
  double lowestEndTime     = 0.0;
  double highestEndTime    = 0.0;
  int timeouts             = 0;
  int completed            = 0;
  for (int i = 0; i < nbruns; i++) {
    if (i > 0) {
      SIMIX_reinit();
    }
    SIMIX_launch_application(deploy);
    try {
      SIMIX_run();
      simulationEndTime = SIMIX_get_clock();
      if (completed == 0 || simulationEndTime < lowestEndTime) {
        lowestEndTime = simulationEndTime;
      }
      if (simulationEndTime > highestEndTime) {
        highestEndTime = simulationEndTime;
      }
      completed += 1;
      XBT_CINFO(statmc, "Ending time (for run %i) was %f.", i, simulationEndTime);
      averageEndTime = averageEndTime * completed / (completed + 1) + simulationEndTime / (completed + 1);
    } catch (simgrid::SimulationTimeoutError& ex) {
      timeouts++;
    }
    printf("\r%i/%i simulation(s) performed (%i failed), current estimate for average simulation time: %f. Range: [%f "
           "; %f]",
           i, nbruns, timeouts, averageEndTime, lowestEndTime, highestEndTime);
  }
  printf("The average simulation time for completed runs was %f, there was %i completed runs out of %i.\n",
         averageEndTime, completed, nbruns);
  printf("The simulation times for completed runs ranged from %f to %f", lowestEndTime, highestEndTime);
}

} // namespace statmc
} // namespace simgrid
