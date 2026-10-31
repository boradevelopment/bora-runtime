// Apart of the BORA Source which uses the TAOSU License
// Check LICENSE.md for more information regarding the BORA license.
/*
 * FileName: DomainSocketFactory.h
 * Purpose: ?
*/
#pragma once

class IDomainSocketServer;
class IDomainSocket;

class DomainSocketFactory
{
public:
    // Create a server listening on the specified path
    static std::unique_ptr<IDomainSocketServer> createServer(const std::string& path);

    // Create a client socket connected to the specified path
    static std::unique_ptr<IDomainSocket> createClient(const std::string& path);
};
