// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorCoreHook.h"

SUPERVISOR_REGISTER_SERVER(registerSupervisorClientCoreHooks) {
    HookTemplate::Command(SendFileDescriptor, SupervisorCoreHook::set_wasm_fd);
    HookTemplate::Command(Ping, SupervisorCoreHook::ping);
    HookTemplate::OnConnect(SupervisorCoreHook::on_connect_init);
    HookTemplate::OnDisconnect(SupervisorCoreHook::on_disconnect);
}
