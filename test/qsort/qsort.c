// C Program to illustrate Generic Quicksort Function
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// function for comparing two strings. This function
// is passed as a parameter to _quickSort() when we
// want to sort
int cmpstr(void *v1, void *v2) {
    // casting v1 to char** and then assigning it to
    // pointer to v1 as v1 is array of characters i.e
    // strings.
    char *a1 = *(char **)v1;
    char *a2 = *(char **)v2;
    return strcmp(a1, a2);
}

// function for comparing two strings
int cmpnum(void *s1, void *s2) {
    // casting s1 to int* so it can be
    // copied in variable a.
    int *a = (int *)s1;
    int *b = (int *)s2;
    if ((*a) > (*b))
        return 1;
    else if ((*a) < (*b))
        return -1;
    else
        return 0;
}

/* you can also write compare function for floats,
        chars, double similarly as integer. */
// function for swap two elements
void swap(void *v1, void *v2, int size) {
    // buffer is array of characters which will
    // store element byte by byte
    char buffer[size];

    // memcpy will copy the contents from starting
    // address of v1 to length of size in buffer
    // byte by byte.
    memcpy(buffer, v1, size);
    memcpy(v1, v2, size);
    memcpy(v2, buffer, size);
}

// v is an array of elements to sort.
// size is the number of elements in array
// left and right is start and end of array
//(*comp)(void*, void*) is a pointer to a function
// which accepts two void* as its parameter
void _qsort(void *v, int size, int left, int right,
            int (*comp)(void *, void *)) {
    void *vt, *v3;
    int i, last, mid = (left + right) / 2;
    if (left >= right)
        return;

    // casting void* to char* so that operations
    // can be done.
    void *vl = (char *)(v + (left * size));
    void *vr = (char *)(v + (mid * size));
    swap(vl, vr, size);
    last = left;
    for (i = left + 1; i <= right; i++) {

        // vl and vt will have the starting address
        // of the elements which will be passed to
        // comp function.
        vt = (char *)(v + (i * size));
        if ((*comp)(vl, vt) > 0) {
            ++last;
            v3 = (char *)(v + (last * size));
            swap(vt, v3, size);
        }
    }
    v3 = (char *)(v + (last * size));
    swap(vl, v3, size);
    _qsort(v, size, left, last - 1, comp);
    _qsort(v, size, last + 1, right, comp);
}
