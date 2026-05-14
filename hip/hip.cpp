//////////////////////////////////////////////////////////////////////////////////////////

#include "hipManager.h"
#include <stdio.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////////////////////

static const int rollNumber = 245826;
static const int hipNumber = 1;

int main() {
    printf("[HIP%d] starting\n", hipNumber);

    HIPManager hip(hipNumber, rollNumber);

    if (!hip.initialize()) {
        fprintf(stderr, "[HIP%d] init failed\n", hipNumber);
        return 1;
    }

    hip.run();
    hip.cleanup();

    printf("[HIP%d] exited cleanly\n", hipNumber);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
