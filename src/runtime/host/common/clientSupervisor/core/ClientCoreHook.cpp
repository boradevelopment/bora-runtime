// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "ClientCoreHook.h"
#include "networking/supervisor/hooks/SupervisorHookRegister.h"
#include "clientSupervisor/HookTemplate.h"

SUPERVISOR_REGISTER_CLIENT(registerClientCoreHooks) {
    HookTemplate::Command(WhoAreYou, ClientCoreHook::who_am_i);
    HookTemplate::Command(GetVersion, ClientCoreHook::get_version);
    HookTemplate::Command(FileDescriptorNotifier, ClientCoreHook::has_received_filedescriptor);
    HookTemplate::OnConnect(ClientCoreHook::on_connect);
    HookTemplate::OnDisconnect(ClientCoreHook::on_disconnect);
}
