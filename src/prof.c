/*
 * PSP Software Development Kit - https://github.com/pspdev
 * vitaprof
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * prof.c - Main profiler code
 *
 * Copyright (c) 2005 urchin <c64psp@gmail.com>
 * Copyright (c) 2025 William Bonnaventure
 *
 */
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "vitagprof.h"

#define	GMON_PROF_ON	0
#define	GMON_PROF_BUSY	1
#define	GMON_PROF_ERROR	2
#define	GMON_PROF_OFF	3

#define GMONVERSION	0x00051879

#include <psp2common/types.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>

/** gmon.out file header */
struct gmonhdr 
{
    int lpc;        /* lowest pc address */
    int hpc;        /* highest pc address */
    int ncnt;       /* size of samples + size of header */
    int version;    /* version number */
    int profrate;   /* profiling clock rate */
    int resv[3];    /* reserved */
};

/** frompc -> selfpc graph */
struct rawarc 
{
    unsigned int frompc;
    unsigned int selfpc;
    long count;
};

/** context */
struct gmonparam 
{
    int state;
    unsigned int lowpc;
    unsigned int highpc;
    unsigned int textsize;

    int narcs;
    struct rawarc *arcs;

    int nsamples;
    unsigned int *samples;

    int thread_id;
        
    unsigned int pc;
};

/// holds context statistics
static struct gmonparam gp;

/// one histogram per four bytes of text space
#define	HISTFRACTION	4

/// define sample frequency - 1000 hz = 1ms
#define SAMPLE_FREQ     1000

/// have we allocated memory and registered already
static int initialized = 0;

/// defined by linker
extern int __executable_start;
extern int __etext;

/* forward declarations */
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void __gprof_cleanup(void);
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void _mcount_internal(unsigned int, unsigned int);
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
static int profiler_thread(SceSize args, void *argp);

/** Initializes pg library

    After calculating the text size, initialize() allocates enough
    memory to allow fastest access to arc structures, and some more
    for sampling statistics. Note that this also installs a timer that
    runs at 1000 hert.
*/
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
static void initialize()
{
    initialized = 1;

    sceClibPrintf("vitagprof: initializing\n");

    memset(&gp, '\0', sizeof(gp));
    gp.state = GMON_PROF_ON;
    gp.lowpc = (unsigned int)&__executable_start;
    gp.highpc = (unsigned int)&__etext;
    gp.textsize = gp.highpc - gp.lowpc;

    gp.narcs = (gp.textsize + HISTFRACTION - 1) / HISTFRACTION;
    gp.arcs = (struct rawarc *)malloc(sizeof(struct rawarc) * gp.narcs);
    if (gp.arcs == NULL)
    {
        gp.state = GMON_PROF_ERROR;
        sceClibPrintf("vitagprof: error allocating %d bytes\n", sizeof(struct rawarc) * gp.narcs);
        return;
    }

    gp.nsamples = (gp.textsize + HISTFRACTION - 1) / HISTFRACTION;
    gp.samples = (unsigned int *)malloc(sizeof(unsigned int) * gp.nsamples);
    if (gp.samples == NULL)
    {
        free(gp.arcs);
        gp.arcs = 0;
        gp.state = GMON_PROF_ERROR;
        sceClibPrintf("vitagprof: error allocating %d bytes\n", sizeof(unsigned int) * gp.nsamples);
        return;
    }

    memset((void *)gp.arcs, '\0', gp.narcs * (sizeof(struct rawarc)));
    memset((void *)gp.samples, '\0', gp.nsamples * (sizeof(unsigned int )));

    sceClibPrintf("vitagprof: %d bytes allocated\n", sizeof(struct rawarc) * gp.narcs + sizeof(unsigned int) * gp.nsamples);

    int thid = 0;
    thid = sceKernelCreateThread("profilerThread", profiler_thread, 0x10000100, 0x10000, 0, 0, NULL);
    if (thid < 0) {
        sceClibPrintf("vitagprof: sceKernelCreateThread failed with error code %i\n", thid);
        free(gp.arcs);
        free(gp.samples);
        gp.arcs = 0;
        gp.samples = 0;
        gp.state = GMON_PROF_ERROR;
        return;
    }

    gp.thread_id = thid;

    sceKernelStartThread(thid, 0, 0);
}

__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void gprof_start(void) {
    // There is already a profiling session running, let's stop it and ignore the result
    if (gp.state == GMON_PROF_ON) {
        gprof_stop(NULL, 0);
    }
    initialize();
}

__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void gprof_stop(const char* filename, int should_dump)
{
    FILE *fp;
    int i;
    struct gmonhdr hdr;

    if (gp.state != GMON_PROF_ON)
    {
        /* profiling was disabled anyway */
        return;
    }

    // Disable profiling to end profiler_thread
    gp.state = GMON_PROF_OFF;

    sceClibPrintf("vitagprof: stopping\n");

    // Wait for end of thread, timeout is 2 seconds
    int exitstatus = 0;
    SceUInt timeout = 2000000;
    int ret = sceKernelWaitThreadEnd(gp.thread_id, &exitstatus, &timeout);
    if (ret < 0 || exitstatus != 0)
    {
        sceClibPrintf("vitagprof: error on profiler exit. Exit status %i, return code %i\n", exitstatus, ret);
    }

    sceKernelDeleteThread(gp.thread_id);

    if (should_dump) {
        sceClibPrintf("vitagprof: dumping data to %s\n", filename);
        fp = fopen(filename, "wb");
        hdr.lpc = gp.lowpc;
        hdr.hpc = gp.highpc;
        hdr.ncnt = sizeof(hdr) + (sizeof(unsigned int) * gp.nsamples);
        hdr.version = GMONVERSION;
        hdr.profrate = SAMPLE_FREQ;
        hdr.resv[0] = 0;
        hdr.resv[1] = 0;
        hdr.resv[2] = 0;
        fwrite(&hdr, 1, sizeof(hdr), fp);
        fwrite(gp.samples, gp.nsamples, sizeof(unsigned int), fp);

        for (i=0; i<gp.narcs; i++)
        {
            if (gp.arcs[i].count > 0)
            {
                // sceClibPrintf("arc frompc %x selfpc %x count %d\n", gp.arcs[i].frompc, gp.arcs[i].selfpc, gp.arcs[i].count);
                fwrite(gp.arcs + i, sizeof(struct rawarc), 1, fp);
            }
        }

        fclose(fp);
    }

    // Free memory
    free(gp.arcs);
    free(gp.samples);
}

/** Writes gmon.out dump file and stops profiling
    Called from atexit() handler; will dump out a gmon.out file 
    at cwd with all collected information.
*/
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void __gprof_cleanup()
{
    // TODO : not called ?
    gprof_stop("ux0:/data/gmon.out", 1);
}

/** Internal C handler for _mcount_internal()
    @param frompc    pc address of caller
    @param selfpc    pc address of current function

    Called from mcount.S to make life a bit easier. _mcount_internal is called
    right before a function starts. GCC generates a tiny stub at the very
    beginning of each compiled routine, which eventually brings the 
    control to here. 
*/
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
void _mcount_internal(unsigned int frompc, unsigned int selfpc)
{ 
    int e;
    struct rawarc *arc;

    if (initialized == 0)
    {
        initialize();
    }

    if (gp.state != GMON_PROF_ON)
    {
        /* returned off for some reason */
        return;
    }

    // Don't ask me why but addresses are 0xB shifted from the elf adresses
    frompc -= 0xB;
    selfpc -= 0xB;

    /* call might come from stack */
    if (frompc >= gp.lowpc && frompc <= gp.highpc)
    {
        gp.pc = selfpc;
        e = (frompc - gp.lowpc) / HISTFRACTION;
        arc = gp.arcs + e;
        arc->frompc = frompc;
        arc->selfpc = selfpc;
        arc->count++;
    }
}

/** Internal profiler thread
*/
__attribute__((__no_instrument_function__, __no_profile_instrument_function__))
static int profiler_thread(SceSize args, void *argp)
{
    while (gp.state == GMON_PROF_ON)
    {
        unsigned int frompc = gp.pc;

        /* call might come from stack */
        if (frompc >= gp.lowpc && frompc <= gp.highpc)
        {
            // Increment sample
            int e = (frompc - gp.lowpc) / HISTFRACTION;
            gp.samples[e]++;
        }

        // Delay until next sample increment
        sceKernelDelayThread(1000 * 1000 / SAMPLE_FREQ);
    }

    return 0;
}
