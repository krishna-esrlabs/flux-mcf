/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_remote/ZmqMsgPackValueReceiver.h"

#include "mcf_core/ErrorMacros.h"
#include "mcf_core/TypeRegistry.h"
#include "mcf_remote/IComEventListener.h"
#include "mcf_remote/Remote.h"
#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/ZmqMsgPackUtils.h"

namespace mcf
{
namespace remote
{
ZmqMsgPackValueReceiver::ZmqMsgPackValueReceiver(
    const std::string& connection,
    TypeRegistry& typeRegistry,
    std::shared_ptr<ShmemClient> shmemClient)
: AbstractZmqMsgPackReceiver(connection, shmemClient), _typeRegistry(typeRegistry)
{
}

ValuePtr
ZmqMsgPackValueReceiver::decodeValue(ZmqMessage& message)
{
    return remote::impl::unpackMessage(_typeRegistry, message.request, message.extMem, message.extMemSize);
}

} // end namespace remote

} // end namespace mcf
