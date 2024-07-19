/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_REMOTE_SERVICE_CONFIGURATOR_H
#define MCF_REMOTE_SERVICE_CONFIGURATOR_H

#include "mcf_core/ComponentType.h"

#include "json/forwards.h"

#include <map>
#include <memory>
#include <string>

namespace mcf
{

class ComponentInstantiator;
class ComponentManager;
class ComponentProxy;
class ValueStore;

namespace remote
{

class RemoteService;
class ShmemClient;
class ShmemKeeper;


/**
 * Remote service configuration from JSON description
 */
class RemoteServiceConfigurator
{

public:

    /**
     * Constructor
     *
     * @param valueStore     The value store to be used by RemoteService instances
     * @param shmemKeeper    Class to manage shared memory in case it is used by any
     *                       RemoteService instance. Using the default
     *                       argument will result in errors if a send rule uses the shm protocol.
     * @param shmemClient    The shared memory client to be used by RemoteService instances.
     *                       Only used with the protocol shm.
     */
    RemoteServiceConfigurator(ValueStore &valueStore,
                              std::shared_ptr<ShmemKeeper> shmemKeeper = std::shared_ptr<ShmemKeeper>(),
                              std::shared_ptr<ShmemClient> shmemClient = nullptr);

    /**
     * Configures a set of remote service instances according to the given JSON config node.
     *
     * The node shall contain the plain configuration data, i.e. without its node name
     * (such as "RemoteServices"). Note that the returned instances will not yet be
     * registered with the component manager.
     *
     * @param node JSON object containing the configuration
     *
     * @return  Map of instance names with shared pointers to the corresponding
     *          RemoteService instances.
     */
    std::map<std::string, std::shared_ptr<RemoteService>>
    configureFromJSONNode(const Json::Value &config);

private:

    ValueStore &fValueStore;
    std::shared_ptr<ShmemKeeper> fShmemKeeper;
    std::shared_ptr<ShmemClient> fShmemClient;
};

} // namespace remote

} // namespace mcf

#endif // MCF_REMOTE_SERVICE_CONFIGURATOR_H