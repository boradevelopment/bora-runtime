// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorClientPermissionHook.h"

SUPERVISOR_REGISTER_SERVER(registerSupervisorClientPermissionsHooks) {
    HookTemplate::Command(GetPermissions, SupervisorClientPermissionHook::get_permission);
    // HookTemplate::Command(Ping, SupervisorClientPermissionHook::ping);
}
