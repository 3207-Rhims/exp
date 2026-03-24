#ifndef CPUCYCLES_H
#define CPUCYCLES_H

#include <stdint.h>

#if defined(__x86_64__) || defined(_M_X64)

  #ifdef USE_RDPMC
  static inline uint64_t cpucycles(void) {
    const uint32_t ecx = (1U << 30) + 1;
    uint64_t result;
    __asm__ volatile (
      "rdpmc; shlq $32,%%rdx; orq %%rdx,%%rax"
      : "=a" (result)
      : "c" (ecx)
      : "rdx"
    );
    return result;
  }
  #else
  static inline uint64_t cpucycles(void) {
    uint64_t result;
    __asm__ volatile (
      "rdtsc; shlq $32,%%rdx; orq %%rdx,%%rax"
      : "=a" (result)
      : 
      : "%rdx"
    );
    return result;
  }
  #endif

#elif defined(__aarch64__) && defined(__APPLE__)
  // Apple Silicon — use Mach absolute time
  #include <mach/mach_time.h>
  static inline uint64_t cpucycles(void) {
    return mach_absolute_time();
  }

#elif defined(__aarch64__)
  // Generic AArch64 (Linux, etc.) — read the virtual count
  static inline uint64_t cpucycles(void) {
    uint64_t result;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r" (result));
    return result;
  }

#else
  // Fallback — POSIX high‑resolution clock
  #include <time.h>
  static inline uint64_t cpucycles(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
  }
#endif

// Optional: you can still implement or keep your overhead measurement
uint64_t cpucycles_overhead(void);

#endif /* CPUCYCLES_H */
