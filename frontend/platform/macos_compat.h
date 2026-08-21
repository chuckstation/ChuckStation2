#ifndef MACOS_COMPAT_H
#define MACOS_COMPAT_H

#if defined(__APPLE__)
#include <time.h>
#include <errno.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

#ifndef HAVE_CS2_CLOCK_NANOSLEEP
#define HAVE_CS2_CLOCK_NANOSLEEP
static inline int cs2_clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *req, struct timespec *rem) {
    if (flags == TIMER_ABSTIME) {
        struct timespec now;
        clock_gettime(clock_id, &now);
        int64_t now_ns = (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
        int64_t req_ns = (int64_t)req->tv_sec * 1000000000LL + req->tv_nsec;
        int64_t diff_ns = req_ns - now_ns;
        if (diff_ns <= 0) return 0;
        struct timespec rel = { (time_t)(diff_ns / 1000000000LL), (long)(diff_ns % 1000000000LL) };
        while (nanosleep(&rel, &rel) < 0) {
            if (errno == EINTR) continue;
            return errno;
        }
        return 0;
    } else {
        while (nanosleep(req, rem) < 0) {
            if (errno == EINTR) continue;
            return errno;
        }
        return 0;
    }
}
#define clock_nanosleep cs2_clock_nanosleep
#endif
#endif

#endif
