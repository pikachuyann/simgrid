/* Copyright (c) 2004-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "xbt/log.h"
#include "xbt/sysdep.h"
#include <ostream>
#include "src/kernel/resource/profile/Profile.hpp"
#include <boost/algorithm/string.hpp>
#include "src/kernel/resource/profile/FutureEvtSet.cpp"
#include "src/kernel/resource/profile/DatedValue.cpp"
#include "src/kernel/resource/profile/StochasticDatedValue.cpp"
#include "simgrid/forward.h"
#include <vector>
#include <regex>

static std::unordered_map<std::string, simgrid::kernel::profile::Profile*> trace_list;

namespace simgrid {
namespace kernel {
namespace profile {

Profile::Profile()
{
  /* Add the first fake event storing the time at which the trace begins */
  DatedValue val(0, -1);
  StochasticDatedValue stoval(0, -1);
  event_list.push_back(val);
  stochastic_event_list.push_back(stoval);
}
Profile::~Profile()          = default;

/** @brief Register this profile for that resource onto that FES,
 * and get an iterator over the integrated trace  */
Event* Profile::schedule(FutureEvtSet* fes, resource::Resource* resource)
{
  Event* event    = new Event();
  event->profile  = this;
  event->idx      = 0;
  event->resource = resource;
  event->free_me  = false;

  xbt_assert((event->idx < event_list.size()), "Your profile should have at least one event!");

  fes_ = fes;
  fes_->add_event(0.0 /* start time */, event);
  if (stochastic > 0) {
    xbt_assert((event->idx < stochastic_event_list.size()), "Your profile should have at least one stochastic event!");
    futureDV = stochastic_event_list.at(event->idx).get_datedvalue();
  }

  return event;
}

/** @brief Gets the next event from a profile */
DatedValue Profile::next(Event* event)
{
  double event_date  = fes_->next_date();

  if (!stochastic) {
	DatedValue dateVal = event_list.at(event->idx);

    if (event->idx < event_list.size() - 1) {
      fes_->add_event(event_date + dateVal.date_, event);
      event->idx++;
    } else if (dateVal.date_ > 0) { /* Last element. Shall we loop? */
      fes_->add_event(event_date + dateVal.date_, event);
      event->idx = 1; /* idx=0 is a placeholder to store when events really start */
    } else {          /* If we don't loop, we don't need this event anymore */
      event->free_me = true;
    }
    return dateVal;
  } else {
    DatedValue dateVal = futureDV;
    if (event->idx < stochastic_event_list.size() - 1) {
	  event->idx++;
	} else if (stochasticloop > 0) { /* We have reached the last element and we have to loop. */
	  event->idx=1;
	} else {
	  event->free_me = true; /* We have reached the last element, but we don't need to loop. */
	}
	  
	if (event->free_me == false) { // In the case there is an element, we draw the next event
	  StochasticDatedValue stodateVal = stochastic_event_list.at(event->idx);
	  futureDV=stochastic_event_list.at(event->idx).get_datedvalue();
	  fes_->add_event(event_date + futureDV.date_, event);
	}
	return dateVal;
  }
}

Profile* Profile::from_string(const std::string& name, const std::string& input, double periodicity)
{
  int linecount                                    = 0;
  simgrid::kernel::profile::Profile* profile       = new simgrid::kernel::profile::Profile();
  simgrid::kernel::profile::DatedValue* last_event = &(profile->event_list.back());

  // We define two buffer values that will serve to parse the various distribution law.
  char tmpA[50];
  char tmpB[50];
  // I am pretty much certain it is not the best solution. Also, the two following regexp may be used:
  std::regex stochasticloop_regexp("STOCHASTICLOOP", std::regex_constants::ECMAScript | std::regex_constants::icase);
  std::regex stochastic_regexp("STOCHASTIC", std::regex_constants::ECMAScript | std::regex_constants::icase);
	
  xbt_assert(trace_list.find(name) == trace_list.end(), "Refusing to define trace %s twice", name.c_str());

  std::vector<std::string> list;
  boost::split(list, input, boost::is_any_of("\n\r"));
  for (auto val : list) {
    simgrid::kernel::profile::DatedValue event;
	simgrid::kernel::profile::StochasticDatedValue stochevent;
    linecount++;
    boost::trim(val);
    if (val[0] == '#' || val[0] == '\0' || val[0] == '%') // pass comments
      continue;
    if (sscanf(val.c_str(), "PERIODICITY %lg\n", &periodicity) == 1)
      continue;
    if (sscanf(val.c_str(), "LOOPAFTER %lg\n", &periodicity) == 1)
      continue;
	if (std::regex_search(val,stochasticloop_regexp)) {
	  profile->stochastic=true; profile->stochasticloop=true;
	  continue;
	}
	if (std::regex_search(val,stochastic_regexp)) {
	  profile->stochastic=true;
	  continue;
	}
	
    // We keep the previous parser untouched if we are not dealing with a stochastic profile
	if (not profile->stochastic) {
      XBT_ATTRIB_UNUSED int res = sscanf(val.c_str(), "%lg  %lg\n", &event.date_, &event.value_);
      xbt_assert(res == 2, "%s:%d: Syntax error in trace\n%s", name.c_str(), linecount, input.c_str());

      xbt_assert(last_event->date_ <= event.date_,
                 "%s:%d: Invalid trace: Events must be sorted, but time %g > time %g.\n%s", name.c_str(), linecount,
                 last_event->date_, event.date_, input.c_str());
      last_event->date_ = event.date_ - last_event->date_;

      profile->event_list.push_back(event);
      last_event = &(profile->event_list.back());
	} else {
	  if (sscanf(val.c_str(), "%lg %lg\n", &event.date_, &event.value_) == 2) {
		// In the case no law is given, we consider it is a deterministic event bound to happen [given date] time AFTER the previous event.
	    stochevent.date_law="DET"; stochevent.date_params={event.date_};
		stochevent.value_law="DET"; stochevent.value_params={event.value_};
	  }
	  else if (sscanf(val.c_str(), "%s %lg %s %lg\n", &tmpA[0], &event.date_, &tmpB[0], &event.value_) == 4) {
	    stochevent.date_law=tmpA; stochevent.date_params={event.date_};
		stochevent.value_law=tmpB; stochevent.value_params={event.value_};
	  }
	  else {
	    // ToDo: A more general approach for parsing STRA v1 v2 v3 v4 v5 STRB v6 v7 v8 values
		xbt_assert(false,"Could not yet parse the given profile");
	  }
	  profile->stochastic_event_list.push_back(stochevent);
	}
  }
  if (last_event) {
    if (periodicity > 0) {
      last_event->date_ = periodicity + profile->event_list.at(0).date_;
    } else {
      last_event->date_ = -1;
    }
  }

  trace_list.insert({name, profile});

  return profile;
}
Profile* Profile::from_file(const std::string& path)
{
  xbt_assert(not path.empty(), "Cannot parse a trace from an empty filename");
  xbt_assert(trace_list.find(path) == trace_list.end(), "Refusing to define trace %s twice", path.c_str());

  std::ifstream* f = surf_ifsopen(path);
  xbt_assert(not f->fail(), "Cannot open file '%s' (path=%s)", path.c_str(), (boost::join(surf_path, ":")).c_str());

  std::stringstream buffer;
  buffer << f->rdbuf();
  delete f;

  return Profile::from_string(path, buffer.str(), -1);
}
	
} // namespace profile
} // namespace kernel
} // namespace simgrid
