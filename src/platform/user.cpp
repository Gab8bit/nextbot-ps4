#include "user.h"

#include <stdio.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <orbis/UserService.h>

int user_init() {
    OrbisUserServiceInitializeParams p;
    memset(&p, 0, sizeof(p));
    p.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    int rc = sceUserServiceInitialize(&p);
    if (rc != 0) printf("[nextbot] sceUserServiceInitialize: %x\n", rc);
    int32_t userId = -1;
    rc = sceUserServiceGetInitialUser(&userId);
    if (rc != 0) {
        printf("[nextbot] sceUserServiceGetInitialUser failed: %x\n", rc);
        return -1;
    }
    return userId;
}

bool user_get_name(int userId, char* buf, int len) {
    if (!buf || len <= 0) return false;
    // ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH is 16 characters, plus terminator
    char name[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH + 16];
    memset(name, 0, sizeof(name));
    if (userId >= 0 && sceUserServiceGetUserName(userId, name, sizeof(name) - 1) == 0 && name[0]) {
        snprintf(buf, len, "%s", name);
        return true;
    }
    snprintf(buf, len, "Player");
    return false;
}

void user_shutdown() {
    sceUserServiceTerminate();
}
