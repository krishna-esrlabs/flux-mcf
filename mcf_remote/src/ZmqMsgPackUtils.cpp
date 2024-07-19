/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/ZmqMsgPackUtils.h"
#include "mcf_core/LoggingMacros.h"

#include "mcf_core/ErrorMacros.h"

namespace mcf {

namespace remote {

void parseConnectionName(
    const std::string& connection,
    std::string& socketName,
    std::string& shmemFile)
{
    std::string shmemPrefix("shm://");
    try
    {
        if(connection.substr(0, shmemPrefix.size()).compare(shmemPrefix))
        {
            // shm not used, use connection as socket name
            socketName = connection;
            return;
        }

        // shm is passed as connection
        // generate ipc socket name
        shmemFile = connection.substr(shmemPrefix.size());
        socketName = std::string("ipc:///tmp/") + shmemFile;
    }
    catch(const std::exception& e)
    {
        MCF_THROW_RUNTIME(std::string("parseConnectionName failed: ") + e.what());
    }

}

void
ZmqMsgPackMessageReceiver::receive(
    const std::string& topic, const std::function<void(ZmqMessage&)>& handleMessage)
{
    if (_shmemFileName.empty())
    {
        remote::receiveMessage(handleMessage, _socket);
    }
    else
    {
#ifdef HAVE_SHMEM
        if (_shmemClient == nullptr)
#endif
        {
            MCF_ERROR_NOFILELINE(
                "No ShmemClient was set. Cannot receive value on {} over shm://{}",
                topic,
                _shmemFileName);
            return;
        }
#ifdef HAVE_SHMEM
        remote::receiveMessage(handleMessage, _socket, _shmemClient);
#endif
    }
}

} // end namespace remote

} // end namespace mcf

