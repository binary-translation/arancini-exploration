#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    if (argc < 2)
        return -1;

    unsigned n = atoi(argv[1]) - 1;

    unsigned prev = 1;
    unsigned pprev = 0;
    for (unsigned i = 0; i < n; i++) {

        unsigned tmp = pprev;
        pprev = prev;
        prev = tmp + pprev;
    }

    printf("Result: %u\n", prev);
    return 0;
}