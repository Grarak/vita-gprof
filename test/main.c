#include <stdio.h>
#include <string.h>

#include <vitagprof.h>

#include <psp2/kernel/clib.h> 
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>


unsigned int compute(unsigned int a, unsigned int b)
{
    // Do something that takes time
    unsigned int result = a;
    result *= b;
    result >>= 5;
    result %= 77777;
    result <<= 7;
    result /= 83;
    return result;
}

/* main routine */
int main(int argc, char *argv[])
{
    sceClibPrintf("PS Vita Profiler Test v0.1\n\n");

    unsigned int a = 7, b = 2, result = 0;

    SceRtcTick start_tick, current_tick;
    sceRtcGetCurrentTick(&start_tick);

    do
    {
        result = compute(a, b);
        b = result;
        result = compute(a, b);
        a = result;
        sceRtcGetCurrentTick(&current_tick);
    } while (current_tick.tick - start_tick.tick < 10000000);

    gprof_stop("ux0:/data/gmon.out", 1);

	return 0;
}
