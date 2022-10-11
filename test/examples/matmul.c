#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    if (argc < 2)
        return -1;

    // Deal with it
    srand(0);

    unsigned dim = atoi(argv[1]);
    double *A = malloc(dim*dim*sizeof(double));
    double *B = malloc(dim*dim*sizeof(double));
    double *C = calloc(dim*dim, sizeof(double));

    if (!A | !B)
        return -1;

    for (unsigned i = 0; i < dim*dim; i++) {

        A[i] = (rand()%10)/(rand()%10);
        B[i] = (rand()%10)/(rand()%10);
    }

    for (unsigned i = 0; i < dim; i++) {
        for (unsigned k = 0; k < dim; k++) {
            for (unsigned j = 0; j < dim; j++) {
                C[i*dim+j] += A[i*dim+k]*B[k*dim+j];
            }
        }
    }

    return 0;
}