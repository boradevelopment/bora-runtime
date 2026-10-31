// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorClientFSHook.h"

SUPERVISOR_REGISTER_SERVER(registerSupervisorClientFSHooks) {
    HookTemplate::Command(OpenFile, SupervisorClientFSHook::open_file);
    HookTemplate::Command(CloseFile, SupervisorClientFSHook::close_file);
    HookTemplate::Command(ReadFile, SupervisorClientFSHook::read_file);
    HookTemplate::Command(SeekFile, SupervisorClientFSHook::seek_file);
}
