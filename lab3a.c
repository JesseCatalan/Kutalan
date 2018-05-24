#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "ext2_fs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid invocation! ./lab3a [image]\n");
        exit(1);
    }

    char *img_name = argv[1];
    printf("%s\n", img_name);
    return 0;
}
