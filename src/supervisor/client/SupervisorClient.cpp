// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "SupervisorClient.h"

#include <sys/mman.h>
#include <sys/stat.h>

#include "memory/MappedMemoryFactory.h"

SupervisorDependency::SupervisorDependency(int fd)
{
    archiveMemoryMapping = std::move(MappedMemoryFactory::filedescriptor(fd));
}

SupervisorClient::SupervisorClient(int clientID) : clientId(clientID)
{
}

sString SupervisorClient::getAppId()
{
    return appId;
}

SupervisorDependency* SupervisorClient::getDependency(const sString& id)
{
    auto it = dependencies_.find(id);
    if (it == dependencies_.end()) return nullptr;

    return &it->second;
}

IMappedMemory* SupervisorClient::getArchiveMapping() const
{
    return archiveMapping.get();
}

IMappedMemory* SupervisorClient::getWasmMemoryMapping() const
{
    return wasmMemoryMapping.get();
}

void SupervisorClient::setFileDescriptor(int fd)
{
    wasmMemoryMapping = std::move(MappedMemoryFactory::filedescriptor(fd, 0x2000000));
}

void SupervisorClient::setFileDescriptorArchive(int fd)
{
    archiveMapping = std::move(MappedMemoryFactory::filedescriptor(fd));
}

void SupervisorClient::addDependency(const sString& id, int fd)
{
    dependencies_.emplace(id, SupervisorDependency(fd));
}

void SupervisorClient::setAppId(const sString& id)
{
    appId = id;
}

