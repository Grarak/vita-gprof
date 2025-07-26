#include <stdio.h>
#include <string.h>

#include <vitagprof.h>

#include <psp2/kernel/clib.h> 
#include <psp2/kernel/threadmgr.h>


unsigned int compute(unsigned int a, unsigned int b)
{
    // Do something that takes time
    unsigned int result = a;
    result *= b;
    return result;
}

/* main routine */
int main(int argc, char *argv[])
{
	sceClibPrintf("PS Vita Profiler Test v0.1\n\n");

    unsigned int a = 7, b = 2, result = 0;
    int i = 10;

	while (i-- > 0)
	{
        result = compute(a, b);

        sceKernelDelayThread(1000000); // Wait for 1 second
	}

    gprof_stop("ux0:/data/gmon.out", 1);

	return 0;
}
