/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_ICOMPONENTCONFIG_H
#define MCF_ICOMPONENTCONFIG_H

#include <string>

#include "mcf_core/ValueStore.h"
#include "mcf_core/Messages.h"

namespace mcf {

class Port;

class IComponentConfig {
public:
    virtual ~IComponentConfig() = default;

    /**
     * @brief Register a port object
     *
     * The implementation registers the supplied port object with the managing infrastructure. When
     * registered, a port is made known and accessible outside of a component and its properties can
     * be changed from outside. Registration is required to be able to connect the port to the
     * messaging layer.
     *
     * @param port The port object that should be registered
     */
    virtual void registerPort(Port& port) = 0;
    
    /**
     * @brief Register a port object and attach it to a topic
     *
     * The method extends IComponentConfig::registerPort(Port&) with attaching the port object to a
     * topic. Note that this mapping can be changed afterwards. The component should thus not rely
     * on specific topic names even if it specifies them via this method.
     *
     * When deciding whether to default-attach a port to topic or not, it might be useful to decide
     * whether the owning component is internal to some semantically closed pipeline or an endpoint
     * of one. For example, in the path planning pipeline, a driving fence/drivable area processing
     * might take place which is internal to the pipeline and thus, there is no need for
     * re-attaching a topic and defining a default topic inside of the component can be helpful.
     * Another use case could be singleton components where the input/output topics can be part of
     * the external interface.
     *
     * If, on the other hand, the component is designed to be used in different contexts (such as a
     * synchronizer which receives values on several topics and re-emits them with synchronized
     * timestamps), then it is useful not to default-attach and force the user of the component to
     * explicitly map the ports to the desired topics.
     *
     * @param port The port object
     * @param topic The topic name to attach to
     */
    virtual void registerPort(Port& port, const std::string& topic) = 0;

    virtual std::string instanceName() const = 0;
};

} // namespace mcf

#endif // MCF_ICOMPONENTCONFIG_H

