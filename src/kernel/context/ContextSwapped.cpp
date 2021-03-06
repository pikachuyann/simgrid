/* Copyright (c) 2009-2019. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "simgrid/Exception.hpp"
#include "simgrid/modelchecker.h"
#include "src/internal_config.h"
#include "src/kernel/actor/ActorImpl.hpp"
#include "src/kernel/context/context_private.hpp"
#include "src/simix/smx_private.hpp"
#include "xbt/parmap.hpp"

#include "src/kernel/context/ContextSwapped.hpp"

#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#ifdef __MINGW32__
#define _aligned_malloc __mingw_aligned_malloc
#define _aligned_free __mingw_aligned_free
#endif /*MINGW*/

#if HAVE_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

XBT_LOG_EXTERNAL_DEFAULT_CATEGORY(simix_context);

namespace simgrid {
namespace kernel {
namespace context {

/* thread-specific storage for the worker's context */
thread_local SwappedContext* SwappedContext::worker_context_ = nullptr;

SwappedContextFactory::SwappedContextFactory() : ContextFactory(), parallel_(SIMIX_context_is_parallel())
{
  parmap_ = nullptr; // will be created lazily with the right parameters if needed (ie, in parallel)
}

SwappedContext::SwappedContext(std::function<void()>&& code, smx_actor_t actor, SwappedContextFactory* factory)
    : Context(std::move(code), actor), factory_(factory)
{
  // Save maestro (=context created first) in preparation for run_all
  if (not factory->parallel_ && factory->maestro_context_ == nullptr)
    factory->maestro_context_ = this;

  if (has_code()) {
    xbt_assert((smx_context_stack_size & 0xf) == 0, "smx_context_stack_size should be multiple of 16");
    if (smx_context_guard_size > 0 && not MC_is_active()) {

#if !defined(PTH_STACKGROWTH) || (PTH_STACKGROWTH != -1)
      xbt_die(
          "Stack overflow protection is known to be broken on your system: you stacks grow upwards (or detection is "
          "broken). "
          "Please disable stack guards with --cfg=contexts:guard-size:0");
      /* Current code for stack overflow protection assumes that stacks are growing downward (PTH_STACKGROWTH == -1).
       * Protected pages need to be put after the stack when PTH_STACKGROWTH == 1. */
#endif

      size_t size = smx_context_stack_size + smx_context_guard_size;
#if SIMGRID_HAVE_MC
      /* Cannot use posix_memalign when SIMGRID_HAVE_MC. Align stack by hand, and save the
       * pointer returned by xbt_malloc0. */
      unsigned char* alloc = static_cast<unsigned char*>(xbt_malloc0(size + xbt_pagesize));
      stack_               = alloc - (reinterpret_cast<uintptr_t>(alloc) & (xbt_pagesize - 1)) + xbt_pagesize;
      reinterpret_cast<unsigned char**>(stack_)[-1] = alloc;
#elif !defined(_WIN32)
      void* alloc;
      if (posix_memalign(&alloc, xbt_pagesize, size) != 0)
        xbt_die("Failed to allocate stack.");
      this->stack_ = static_cast<unsigned char*>(alloc);
#else
      this->stack_ = static_cast<unsigned char*>(_aligned_malloc(size, xbt_pagesize));
#endif

#ifndef _WIN32
      if (mprotect(this->stack_, smx_context_guard_size, PROT_NONE) == -1) {
        xbt_die(
            "Failed to protect stack: %s.\n"
            "If you are running a lot of actors, you may be exceeding the amount of mappings allowed per process.\n"
            "On Linux systems, change this value with sudo sysctl -w vm.max_map_count=newvalue (default value: 65536)\n"
            "Please see "
            "https://simgrid.org/doc/latest/Configuring_SimGrid.html#configuring-the-user-code-virtualization for more "
            "information.",
            strerror(errno));
        /* This is fatal. We are going to fail at some point when we try reusing this. */
      }
#endif
      this->stack_ = this->stack_ + smx_context_guard_size;
    } else {
      this->stack_ = static_cast<unsigned char*>(xbt_malloc0(smx_context_stack_size));
    }

#if PTH_STACKGROWTH == -1
    ASAN_ONLY(this->asan_stack_ = this->stack_ + smx_context_stack_size);
#else
    ASAN_ONLY(this->asan_stack_ = this->stack_);
#endif
#if HAVE_VALGRIND_H
    if (RUNNING_ON_VALGRIND)
      this->valgrind_stack_id_ = VALGRIND_STACK_REGISTER(this->stack_, this->stack_ + smx_context_stack_size);
#endif
  }
}

SwappedContext::~SwappedContext()
{
  if (stack_ == nullptr) // maestro has no extra stack
    return;

#if HAVE_VALGRIND_H
  if (RUNNING_ON_VALGRIND)
    VALGRIND_STACK_DEREGISTER(valgrind_stack_id_);
#endif

#ifndef _WIN32
  if (smx_context_guard_size > 0 && not MC_is_active()) {
    stack_ = stack_ - smx_context_guard_size;
    if (mprotect(stack_, smx_context_guard_size, PROT_READ | PROT_WRITE) == -1) {
      XBT_WARN("Failed to remove page protection: %s", strerror(errno));
      /* try to pursue anyway */
    }
#if SIMGRID_HAVE_MC
    /* Retrieve the saved pointer.  See SIMIX_context_stack_new above. */
    stack_ = reinterpret_cast<unsigned char**>(stack_)[-1];
#endif
  }
#endif /* not windows */

  xbt_free(stack_);
}

unsigned char* SwappedContext::get_stack()
{
  return stack_;
}

void SwappedContext::stop()
{
  Context::stop();
  /* We must cut the actor execution using an exception to properly free the C++ RAII variables */
  throw ForcefulKillException();
}

/** Maestro wants to run all ready actors */
void SwappedContextFactory::run_all()
{
  /* This function is called by maestro at the beginning of a scheduling round to get all working threads executing some
   * stuff It is much easier to understand what happens if you see the working threads as bodies that swap their soul
   * for the ones of the simulated processes that must run.
   */
  if (parallel_) {
    // We lazily create the parmap so that all options are actually processed when doing so.
    if (parmap_ == nullptr)
      parmap_.reset(
          new simgrid::xbt::Parmap<smx_actor_t>(SIMIX_context_get_nthreads(), SIMIX_context_get_parallel_mode()));

    // Usually, Parmap::apply() executes the provided function on all elements of the array.
    // Here, the executed function does not return the control to the parmap before all the array is processed:
    //   - suspend() should switch back to the worker_context (either maestro or one of its minions) to return
    //     the control to the parmap. Instead, it uses parmap_->next() to steal another work, and does it directly.
    //     It only yields back to worker_context when the work array is exhausted.
    //   - So, resume() is only launched from the parmap for the first job of each minion.
    parmap_->apply(
        [](smx_actor_t process) {
          SwappedContext* context = static_cast<SwappedContext*>(process->context_.get());
          context->resume();
        },
        simix_global->actors_to_run);
  } else { // sequential execution
    if (simix_global->actors_to_run.empty())
      return;

    /* maestro is already saved in the first slot of workers_context_ */
    smx_actor_t first_actor = simix_global->actors_to_run.front();
    process_index_          = 1;
    /* execute the first actor; it will chain to the others when using suspend() */
    static_cast<SwappedContext*>(first_actor->context_.get())->resume();
  }
}

/** Maestro wants to yield back to a given actor, so awake it on the current thread
 *
 * In parallel, it is only applied to the N first elements of the parmap array,
 * where N is the amount of worker threads in the parmap.
 * See SwappedContextFactory::run_all for details.
 */
void SwappedContext::resume()
{
  if (factory_->parallel_) {
    // Save my current soul (either maestro, or one of the minions) in a thread-specific area
    worker_context_ = static_cast<SwappedContext*>(self());
    // Switch my soul and the actor's one
    Context::set_current(this);
    worker_context_->swap_into(this);
    // No body runs that soul anymore at this point, but it is stored in a safe place.
    // When the executed actor will do a blocking action, ActorImpl::yield() will call suspend(), below.
  } else { // sequential execution
    SwappedContext* old = static_cast<SwappedContext*>(self());
    Context::set_current(this);
    old->swap_into(this);
  }
}

/** The actor wants to yield back to maestro, because it is blocked in a simcall (i.e., in ActorImpl::yield())
 *
 * Actually, it does not really yield back to maestro, but directly into the next executable actor.
 *
 * This makes the parmap::apply awkward (see ParallelUContext::run_all()) because it only apply regularly
 * on the few first elements of the array, but it saves a lot of context switches back to maestro,
 * and directly forth to the next executable actor.
 */
void SwappedContext::suspend()
{
  if (factory_->parallel_) {
    // Get some more work to directly swap into the next executable actor instead of yielding back to the parmap
    boost::optional<smx_actor_t> next_work = factory_->parmap_->next();
    SwappedContext* next_context;
    if (next_work) {
      // There is a next soul to embody (ie, another executable actor)
      XBT_DEBUG("Run next process");
      next_context = static_cast<SwappedContext*>(next_work.get()->context_.get());
    } else {
      // All actors were run, go back to the parmap context
      XBT_DEBUG("No more actors to run");
      // worker_context_ is my own soul, stored in thread_local when starting the scheduling round
      next_context = worker_context_;
      // When given that soul, the body will wait for the next scheduling round
    }

    // Get the next soul to run, either from another actor or the initial minion's one
    Context::set_current(next_context);
    this->swap_into(next_context);

  } else { // sequential execution
    /* determine the next context */
    SwappedContext* next_context;
    unsigned long int i = factory_->process_index_;
    factory_->process_index_++;

    if (i < simix_global->actors_to_run.size()) {
      /* Actually swap into the next actor directly without transiting to maestro */
      XBT_DEBUG("Run next actor");
      next_context = static_cast<SwappedContext*>(simix_global->actors_to_run[i]->context_.get());
    } else {
      /* all processes were run, actually return to maestro */
      XBT_DEBUG("No more actors to run");
      next_context = factory_->maestro_context_;
    }
    Context::set_current(next_context);
    this->swap_into(next_context);
  }
}

} // namespace context
} // namespace kernel
} // namespace simgrid
