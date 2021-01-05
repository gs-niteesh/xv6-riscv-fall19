#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    printf("Free pages: %d\n", get_free());
    exit(0);
}