#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    sf_mem_init();

    sf_snapshot();
    void * x = sf_malloc(sizeof(int));
    sf_snapshot();
    void * y = sf_malloc(8144);
    sf_snapshot();
    sf_blockprint(x-8);
    sf_blockprint(y-8);
    sf_free(x);
    sf_free(y);
    sf_snapshot();


    sf_mem_fini();
    return EXIT_SUCCESS;
}
