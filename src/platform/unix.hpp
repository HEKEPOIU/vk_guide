#if defined (__unix__) || defined(__linux__)
#include <unistd.h> 
#include <limits.h> 
#elif defined(__APPLE__)

#include <mach-o/dyld.h>

#endif
