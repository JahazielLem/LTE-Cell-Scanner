// Small OS-compatibility shims for LTE-Cell-Scanner.
// Released under the AGPLv3 license, like the rest of the project.

#ifndef LTS_COMPAT_H
#define LTS_COMPAT_H

#include <stdint.h>

// Portable "get current thread id" (informational only; used for display).
#if defined(__APPLE__)
  #include <pthread.h>
  static inline long lts_gettid(void) {
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return (long)tid;
  }
#elif defined(__linux__)
  #include <unistd.h>
  #include <sys/syscall.h>
  static inline long lts_gettid(void) {
    return (long)syscall(SYS_gettid);
  }
#else
  static inline long lts_gettid(void) { return 0; }
#endif

#endif // LTS_COMPAT_H
