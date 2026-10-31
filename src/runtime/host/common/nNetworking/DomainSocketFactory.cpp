// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
#include "DomainSocketFactory.h"
#ifdef __POSIX__
#include "nNetworking/PosixDomainSocket.h"
#endif

std::unique_ptr<IDomainSocketServer> DomainSocketFactory::createServer(const std::string& path)
{
#ifdef __POSIX__
    return std::make_unique<PosixDomainSocketServer>(path);
#endif

    return nullptr; // unsupported
}

std::unique_ptr<IDomainSocket> DomainSocketFactory::createClient(const std::string& path)
{
#ifdef __POSIX__
    return std::make_unique<PosixDomainSocket>(path);
#endif

    return nullptr; // unsupported
}
