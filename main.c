#include "util/so_stdio.h"
#include <stdio.h>

int main() {
    SO_FILE *file = so_fopen("here", "a+");
    char buff[100];
    so_fseek(file, 0, SEEK_SET);
    so_fread(buff, 1, 1, file);
    so_fputc('b', file);
    so_fclose(file);
    printf("%s", buff);
    return 0;
}