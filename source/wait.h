#ifndef _WAIT_H
#define _WAIT_H
#include <unistd.h>
#include <stdatomic.h>

// Efficient thread sleeping/waking methods used here
// Darwin XNU: The 'undocumented' ulock syscalls (Working as of XNU 11417.101.15)
// Linux: Futex syscalls

#ifdef __DARWIN__
#define UL_COMPARE_AND_WAIT  1
#define ULF_NO_ERRNO       0x01000000
#define SYS_ulock_wait     515
#define SYS_ulock_wake     516

static inline int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout) {
    return syscall(SYS_ulock_wait, operation, addr, value, timeout);
}

static inline int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value) {
    return syscall(SYS_ulock_wake, operation, addr, wake_value);
}
#elif __linux__
// TODO
#endif

void waitHandler_wait(atomic_int* addr);
void waitHandler_wake(atomic_int* addr);

#endif
