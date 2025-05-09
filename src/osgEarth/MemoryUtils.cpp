/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/
#include "MemoryUtils"

using namespace osgEarth::Util;

/*
 * ADAPTED FROM:
 * http://nadeausoftware.com/articles/2012/07/c_c_tip_how_get_process_resident_set_size_physical_memory_use
 *
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
//#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif


/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes, or zero if the value cannot be
 * determined on this OS.
 */
std::int64_t
Memory::getProcessPeakPhysicalUsage()
{
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
    return (std::int64_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
    /* AIX and Solaris ------------------------------------------ */
    struct psinfo psinfo;
    int fd = -1;
    if ( (fd = open( "/proc/self/psinfo", O_RDONLY )) == -1 )
        return (std::int64_t)0L;      /* Can't open? */
    if ( read( fd, &psinfo, sizeof(psinfo) ) != sizeof(psinfo) )
    {
        close( fd );
        return (std::int64_t)0L;      /* Can't read? */
    }
    close( fd );
    return (std::int64_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    /* BSD, Linux, and OSX -------------------------------------- */
    struct rusage rusage;
    getrusage( RUSAGE_SELF, &rusage );
#if defined(__APPLE__) && defined(__MACH__)
    return (std::int64_t)rusage.ru_maxrss;
#else
    return (std::int64_t)(rusage.ru_maxrss * 1024L);
#endif

#else
    /* Unknown OS ----------------------------------------------- */
    return (std::int64_t)0L;          /* Unsupported. */
#endif
}



/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
std::int64_t
Memory::getProcessPhysicalUsage()
{
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS_EX info;
    GetProcessMemoryInfo( GetCurrentProcess( ), (PROCESS_MEMORY_COUNTERS*)&info, sizeof(info) );
    return (std::int64_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
    /* OSX ------------------------------------------------------ */
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
        (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
        return (std::int64_t)0L;      /* Can't access? */
    return (std::int64_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    /* Linux ---------------------------------------------------- */
    long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        return (std::int64_t)0L;      /* Can't open? */
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        return (std::int64_t)0L;      /* Can't read? */
    }
    fclose( fp );
    return (unsigned) ((std::int64_t)rss * (std::int64_t)sysconf( _SC_PAGESIZE));

#else
    /* AIX, BSD, Solaris, and Unknown OS ------------------------ */
    return (std::int64_t)0L;          /* Unsupported. */
#endif
}

std::int64_t
Memory::getProcessPrivateUsage()
{
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS_EX info;
    GetProcessMemoryInfo( GetCurrentProcess( ), (PROCESS_MEMORY_COUNTERS*)&info, sizeof(info) );
    return (std::int64_t)info.PrivateUsage;
#else
    return (std::int64_t)0L;
#endif
}


std::int64_t
Memory::getProcessPeakPrivateUsage()
{
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS_EX info;
    GetProcessMemoryInfo( GetCurrentProcess( ), (PROCESS_MEMORY_COUNTERS*)&info, sizeof(info) );
    return (std::int64_t)info.PeakPagefileUsage;
#else
    return (std::int64_t)0L;
#endif
}
