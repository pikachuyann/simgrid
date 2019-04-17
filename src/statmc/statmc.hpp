/* Copyright (c) 2008-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef STATMC_HPP
#define STATMC_HPP

namespace simgrid {
namespace statmc {
void multirun(int);
void multirun(int, const std::string&);
} // namespace statmc
} // namespace simgrid
#endif