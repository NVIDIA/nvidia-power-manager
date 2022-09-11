#include <systemd/sd-bus.h>

// Message Register Variables
#define BUFFER_LENGTH 1024

#define BUSCTL_COMMAND "busctl"
#define LOG_SERVICE "xyz.openbmc_project.Logging"
#define LOG_PATH "/xyz/openbmc_project/logging"
#define LOG_CREATE_INTERFACE "xyz.openbmc_project.Logging.Create"
#define LOG_CREATE_FUNCTION "Create"
#define LOG_CREATE_SIGNATURE "ssa{ss}"

/* no return, we will call and fail silently if busctl isn't present */
void emitLogMessage(const char *message, const char *arg0, const char *arg1, const char *severity,
                    const char *resolution);
