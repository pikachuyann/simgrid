#include <simgrid/plugins/load_balancer.h>
#include <simgrid/s4u/Actor.hpp>
#include <smpi/sampi.h>
#include <src/smpi/include/smpi_comm.hpp>
#include <src/smpi/include/smpi_actor.hpp>
#include <xbt/replay.hpp>

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(plugin_pampi, smpi, "Logging specific to the AMPI functions");

extern "C" void* __libc_malloc(size_t size);
extern "C" void* __libc_free(void* ptr);
static std::vector<size_t> memory_size(500, 0); // FIXME cheinrich This needs to be dynamic
static std::map</*address*/ void*, size_t> alloc_table; // Keep track of all allocations

extern "C" void* _sampi_malloc(size_t size)
{
  void* result = __libc_malloc (size); // We need the space here to prevent recursive substitution
  alloc_table.insert({result, size});
  if (not simgrid::s4u::this_actor::is_maestro()) {
    memory_size[simgrid::s4u::this_actor::get_pid()] += size;
  }
  return result;
}

extern "C" void _sampi_free(void* ptr)
{
  size_t alloc_size = alloc_table.at(ptr);
  int my_proc_id    = simgrid::s4u::this_actor::get_pid();
  memory_size[my_proc_id] -= alloc_size;
  __libc_free(ptr);
}

/* FIXME The following contains several times "rank() + 1". This works for one
 * instance, but we need to find a way to deal with this for several instances and
 * for daemons: If we just replace this with the process id, we will get id's that
 * don't start at 0 if we start daemons as well.
 */
int APMPI_Iteration_in(MPI_Comm comm)
{
  smpi_bench_end();
  TRACE_Iteration_in(comm->rank() + 1, new simgrid::instr::NoOpTIData("iteration_in")); // implemented on instr_smpi.c
  smpi_bench_begin();
  return 1;
}

int APMPI_Iteration_out(MPI_Comm comm)
{
  smpi_bench_end();
  TRACE_Iteration_out(comm->rank() + 1, new simgrid::instr::NoOpTIData("iteration_out"));
  smpi_bench_begin();
  return 1;
}

void APMPI_Migrate(MPI_Comm comm)
{
  smpi_bench_end();
  int my_proc_id = simgrid::s4u::this_actor::get_pid();
  TRACE_migration_call(comm->rank() + 1, new simgrid::instr::AmpiMigrateTIData(memory_size[my_proc_id]));
  smpi_bench_begin();
}

int AMPI_Iteration_in(MPI_Comm comm)
{
  return APMPI_Iteration_in(comm);
}

int AMPI_Iteration_out(MPI_Comm comm)
{
  return APMPI_Iteration_out(comm);
}

void AMPI_Migrate(MPI_Comm comm)
{
  APMPI_Migrate(comm);
}