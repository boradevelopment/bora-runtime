// Apart of the BORA Runtime Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.

#include "Commands.h"

inline std::unordered_map<u64, HostCommandListRecord> HostCommandManager::m_activeLists;
u64 HostCommandManager::m_nextHandle = 1;

inline std::vector<OnInitializeHook> HostCommandManager::m_initHooks;
inline std::vector<OnUpdateHook> HostCommandManager::m_updateHooks;