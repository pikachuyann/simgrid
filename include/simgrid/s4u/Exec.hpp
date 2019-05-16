/* Copyright (c) 2017-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef SIMGRID_S4U_EXEC_HPP
#define SIMGRID_S4U_EXEC_HPP

#include <simgrid/forward.h>
#include <simgrid/s4u/Activity.hpp>
#include <xbt/ex.h>

#include <atomic>

namespace simgrid {
namespace s4u {

/** Computation Activity, representing the asynchronous executions.
 *
 * They are generated from this_actor::exec_init() or Host::execute(), and can be used to model pools of threads or
 * similar mechanisms.
 */
class XBT_PUBLIC Exec : public Activity {
  std::string name_             = "";
  double priority_              = 1.0;
  double bound_                 = 0.0;
  double timeout_               = 0.0;
  std::string tracing_category_ = "";
  std::atomic_int_fast32_t refcount_{0};
  Host* host_ = nullptr;

protected:
  Exec();
  virtual ~Exec() = default;

public:
#ifndef DOXYGEN
  Exec(Exec const&) = delete;
  Exec& operator=(Exec const&) = delete;

  friend ExecSeq;
  friend ExecPar;
  friend XBT_PUBLIC void intrusive_ptr_release(Exec* e);
  friend XBT_PUBLIC void intrusive_ptr_add_ref(Exec* e);
#endif
  static xbt::signal<void(Actor const&)> on_start;
  static xbt::signal<void(Actor const&)> on_completion;

  virtual Exec* start() override          = 0;
  virtual double get_remaining_ratio()    = 0;
  virtual ExecPtr set_host(Host* host)    = 0;

  Exec* wait() override;
  Exec* wait_for(double timeout) override;
  bool test() override;

  ExecPtr set_bound(double bound);
  ExecPtr set_name(const std::string& name);
  ExecPtr set_priority(double priority);
  ExecPtr set_tracing_category(const std::string& category);
  ExecPtr set_timeout(double timeout);
  Exec* cancel() override;

  XBT_ATTRIB_DEPRECATED_v324("Please use Exec::wait_for()") void wait(double t) override { wait_for(t); }
};

class XBT_PUBLIC ExecSeq : public Exec {
  double flops_amount_ = 0.0;

  explicit ExecSeq(sg_host_t host, double flops_amount);

public:
  friend XBT_PUBLIC ExecPtr this_actor::exec_init(double flops_amount);

  ~ExecSeq() = default;

  Exec* start() override;

  ExecPtr set_host(Host* host) override;
  Host* get_host();

  double get_remaining() override;
  double get_remaining_ratio() override;
};

class XBT_PUBLIC ExecPar : public Exec {
  std::vector<s4u::Host*> hosts_;
  std::vector<double> flops_amounts_;
  std::vector<double> bytes_amounts_;
  explicit ExecPar(const std::vector<s4u::Host*>& hosts, const std::vector<double>& flops_amounts,
                   const std::vector<double>& bytes_amounts);
  ExecPtr set_host(Host* host) override { return this; }

public:
  ~ExecPar() = default;
  friend XBT_PUBLIC ExecPtr this_actor::exec_init(const std::vector<s4u::Host*>& hosts,
                                                  const std::vector<double>& flops_amounts,
                                                  const std::vector<double>& bytes_amounts);
  double get_remaining() override;
  double get_remaining_ratio() override;
  Exec* start() override;
};

} // namespace s4u
} // namespace simgrid

#endif /* SIMGRID_S4U_EXEC_HPP */
