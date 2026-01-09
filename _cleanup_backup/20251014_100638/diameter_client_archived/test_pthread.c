#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>

int main() {
    pthread_rwlock_t lock;
    printf("pthread_rwlock_t size: %zu\n", sizeof(lock));
    return 0;
}
