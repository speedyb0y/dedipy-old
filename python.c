/*
    TO BE COMPILED AS A STANDALONE PROGRAM
    THIS PROGRAM IS TO TEST THE CODE, SO WE DON'T NEED TO REBUILD PYTHON TO TEST THINGS
*/

#include "python-dedipy.c"

int main (void) {

    dedipy_main();
    dedipy_free(dedipy_malloc(0));
    dedipy_test();

    return 0;
}
