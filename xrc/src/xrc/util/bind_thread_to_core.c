#if defined(_WIN32) || defined(_WIN64)
  // ----------------- WINDOWS --------------------
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#include <stdio.h>
#include <stdint.h>     // for uint32_t
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS

  int get_logical_cores(void)
  {
      SYSTEM_INFO info;
      GetSystemInfo(&info);
      return (int)info.dwNumberOfProcessors; // Typically the count of logical CPUs
  }

  int bind_thread_to_core(uint32_t core_id)
  {
      HANDLE thread = GetCurrentThread();
      if (thread == NULL) {
          fprintf(stderr, "Error: GetCurrentThread() returned NULL.\n");
          return 1;
      }
      // Create the affinity mask by setting the (core_id)th bit
      DWORD_PTR mask = ((DWORD_PTR)1 << core_id);
      DWORD_PTR result = SetThreadAffinityMask(thread, mask);
      if (result == 0) {
          DWORD err = GetLastError();
          fprintf(stderr, "Error: SetThreadAffinityMask failed (GetLastError=%lu).\n", (unsigned long)err);
          return (int)err;
      }
      return 0;
  }

#elif defined(__APPLE__)
  // ----------------- macOS (BEST EFFORT) --------------------
  // - No official CPU affinity. We can hint with Mach thread_policy_set(),
  //   but the OS scheduler may ignore or only partially honor this request.
  #include <unistd.h>           // for sysconf
  #include <sys/types.h>
  #include <sys/sysctl.h>
  #include <mach/mach.h>
  #include <mach/thread_policy.h>
#include <stdio.h>
#include <stdint.h>     // for uint32_t
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS

  int get_logical_cores(void)
  {
      // Prefer HW_AVAILCPU, fallback to HW_NCPU
      int nm[2];
      nm[0] = CTL_HW;
      nm[1] = HW_AVAILCPU;
      int ncpu = 0;
      size_t len = sizeof(ncpu);

      if (sysctl(nm, 2, &ncpu, &len, NULL, 0) != 0 || ncpu < 1)
      {
          // Fallback to HW_NCPU
          nm[1] = HW_NCPU;
          if (sysctl(nm, 2, &ncpu, &len, NULL, 0) != 0 || ncpu < 1)
          {
              // As a last resort, assume 1
              ncpu = 1;
          }
      }
      return ncpu;
  }

  int bind_thread_to_core(uint32_t core_id)
  {
      // We'll do a best-effort attempt using Mach API
      // but macOS does NOT guarantee strict pinning.
      // First, check the range
      int ncpu = get_logical_cores();
      if ((int)core_id >= ncpu) {
          fprintf(stderr, "Error: core_id %u is out of range (max %d).\n", core_id, ncpu - 1);
          return 1;
      }

      // Mach "affinity" policy
      thread_port_t tid = mach_thread_self();
      thread_affinity_policy_data_t policy = { core_id };
      kern_return_t kr = thread_policy_set(tid,
                                           THREAD_AFFINITY_POLICY,
                                           (thread_policy_t)&policy,
                                           THREAD_AFFINITY_POLICY_COUNT);
      // Deallocate the thread port
      mach_port_deallocate(mach_task_self(), tid);

      if (kr != KERN_SUCCESS) {
          fprintf(stderr, "Warning: thread_policy_set failed (kr=%d)\n", kr);
          return (int)kr;
      }

      return 0;
  }

#elif defined(__FreeBSD__)
  // ----------------- FREEBSD --------------------
#include <sys/param.h>
#include <sys/cpuset.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>     // for uint32_t
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS

// Then CPU_ZERO, CPU_SET, cpuset_setaffinity(), etc., are available.


  int get_logical_cores(void)
  {
      long nproc = sysconf(_SC_NPROCESSORS_ONLN);
      if (nproc < 1) {
          return 1; // fallback
      }
      return (int)nproc;
  }

  int bind_thread_to_core(uint32_t core_id)
  {
      int ncpu = get_logical_cores();
      if ((int)core_id >= ncpu) {
          fprintf(stderr, "Error: core_id %u is out of range (max %d).\n", core_id, ncpu - 1);
          return 1;
      }

      cpuset_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(core_id, &cpuset);

      // -1 for current thread (FreeBSD docs)
      int ret = cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                   sizeof(cpuset), &cpuset);
      if (ret != 0) {
          fprintf(stderr, "Error: cpuset_setaffinity failed (errno=%d).\n", errno);
          return errno;
      }
      return 0;
  }

#else
// ----------------- LINUX or Other POSIX-Like --------------------
#define _GNU_SOURCE    // must be the first thing
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Then your code that calls CPU_ZERO / CPU_SET / pthread_setaffinity_np.


int get_logical_cores(
    void
    )
{
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc < 1)
    {
        return 1; // fallback if we can't detect
    }
    return (int)nproc;
}

int bind_thread_to_core(
    uint32_t core_id
    )
{
    int ncpu = get_logical_cores();
    if ((int)core_id >= ncpu)
    {
        fprintf(stderr, "Error: core_id %u is out of range (max %d).\n", core_id, ncpu - 1);
        return 1;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t thread = pthread_self();
    int       ret    = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0)
    {
        fprintf(stderr, "Error: pthread_setaffinity_np failed (%d).\n", ret);
        return ret;
    }
    return 0;
}
#endif  // end platform checks
