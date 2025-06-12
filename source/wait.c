/*
// IoT Controller
// Efficient Thread Sleeping/Waiting Handler
// Goldenkrew3000 2025
// GPLv3
*/

#include "wait.h"

void waitHandler_wait(atomic_int* addr) {
#if __DARWIN__
    _ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, (void*)addr, 0, 0);
#elif __linux__
    // TODO
#endif
}

void waitHandler_wake(atomic_int* addr) {
#if __DARWIN__
    __ulock_wake(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO, (void*)addr, 0);
#elif __linux__
    // TODO
#endif
}
