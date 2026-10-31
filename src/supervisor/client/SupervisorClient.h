// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: SupervisorClient.h
 * Purpose: ?
*/
#pragma once
#include <sys/mman.h> // todo: linux only.
#include <sys/stat.h>

#include "IMappedMemory.h"
#include "app/IAppPermissions.h"


// Dependency for the client, it is stored inside the client and will never be independent.
class SupervisorDependency
{
public:
    SupervisorDependency(int fd);

    IMappedMemory* operator ->() const
    {
        return archiveMemoryMapping.get();
    }

private:
    sUniquePointer<IMappedMemory> archiveMemoryMapping; // Where the archive data memory is stored.
};

class SupervisorClient
{
public:
    SupervisorClient(int clientID);
    sString getAppId();
    SupervisorDependency* getDependency(const sString& id);
    IMappedMemory* getArchiveMapping() const;
    IMappedMemory* getWasmMemoryMapping() const;
    std::shared_ptr<AppPermissions> getPermissions(); // permissions are pointers because they are only 1 permission object per client that are shared and updated.
    void setFileDescriptor(int fd);
    void setFileDescriptorArchive(int fd);
    void addDependency(const sString& id, int fd);
    void setAppId(const sString& id);

private:
    bool authorized = false;
    int clientId;
    sString appId;

    sUnorderedMap<sString, SupervisorDependency> dependencies_;
    sUniquePointer<IMappedMemory> wasmMemoryMapping;
    sUniquePointer<IMappedMemory> archiveMapping;
};

