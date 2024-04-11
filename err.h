#ifndef MIM_ERR_H
#define MIM_ERR_H

#include <stdnoreturn.h>

#define ASSERT_SYS_OK(expr)                                                                \
    do {                                                                                   \
        if ((expr) < 0)                                                                    \
            syserr(                                                                        \
                "system command failed: %s\n\tIn function %s() in %s line %d.\n\tErrno: ", \
                #expr, __func__, __FILE__, __LINE__                                        \
            );                                                                             \
    } while(0)

/* Print information about a system error and quits. */
noreturn void syserr(const char* fmt, ...);

/* Print information about an error and quits. */
noreturn void fatal(const char* fmt, ...);

#endif
