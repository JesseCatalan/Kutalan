#include "ex2_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid invocation! ./lab3a [image]\n");
        exit(1);
    }

    char *img_name = argv[1];
    printf("%s\n", img_name);
}
