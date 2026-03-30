#include "pico/mutex.h"

void* osMutexInit() {
    mutex_t* m = new mutex_t;
    mutex_init(m);
    return m;
}

void osMutexFree(void* obj) {
    delete static_cast<mutex_t*>(obj);
}

void osMutexLock(void* obj) {
    mutex_enter_blocking(static_cast<mutex_t*>(obj));
}

void osMutexUnlock(void* obj) {
    mutex_exit(static_cast<mutex_t*>(obj));
}

void* osRWLockInit() {
    return osMutexInit();
}

void osRWLockFree(void *obj) {
    osMutexFree(obj);
}

void osRWLockRead(void *obj) {
    osMutexLock(obj);
}

void osRWUnlockRead(void *obj) {
    osMutexUnlock(obj);
}

void osRWLockWrite(void *obj) {
    osMutexLock(obj);
}

void osRWUnlockWrite(void *obj) {
    osMutexUnlock(obj);
}