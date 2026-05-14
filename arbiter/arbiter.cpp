//////////////////////////////////////////////////////////////////////////////////////////

#include "gameArbiter.h"
#include <stdio.h>
#include <stdlib.h>

//////////////////////////////////////////////////////////////////////////////////////////

static const int rollNumber = 245826;

int main() {
    printf("[arbiter] starting - roll no: %d\n", rollNumber);

    GameArbiter arbiter(rollNumber);

    if (!arbiter.initialize()) {
        fprintf(stderr, "[arbiter] init failed - exiting\n");
        return 1;
    }

    arbiter.run();
    arbiter.cleanup();

    printf("[arbiter] exited cleanly\n");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
