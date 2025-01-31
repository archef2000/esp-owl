#include "wifi/core.h"
#include "cli/tasks.h"


#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H
struct systemInfo {
    struct availabeTasks *tasks; // writing to tasks overflows to awdl pointer
    struct daemon_state *awdl;
};
#endif /* SYSTEMINFO_H */