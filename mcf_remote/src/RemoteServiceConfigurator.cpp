/**
 * Copyright (c) 2024 Accenture
 */

#include "mcf_core/ComponentInstantiator.h"
#include "mcf_core/ComponentManager.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/RemoteServiceUtils.h"
#include "mcf_remote/RemoteServiceConfigurator.h"
#include "mcf_remote/ShmemClient.h"
#include "mcf_remote/ShmemKeeper.h"

#include "mcf_core/ErrorMacros.h"
#include "json/json.h"

#include <map>
#include <memory>

namespace mcf
{

namespace remote
{

namespace
{

const char *SEND_CONNECTION_CONFIG_ITEM = "sendConnection";
const char *RECEIVE_CONNECTION_CONFIG_ITEM = "receiveConnection";
const char *SEND_RULES_CONFIG_ITEM = "sendRules";
const char *RECEIVE_RULES_CONFIG_ITEM = "receiveRules";
const char *SENDER_TIMEOUT = "sendTimeout";
const char *SENDER_ARTIFICIAL_JITTER = "artificialJitter";
const char *TOPIC_LOCAL_CONFIG_ITEM = "topic_local";
const char *TOPIC_REMOTE_CONFIG_ITEM = "topic_remote";
const char *SENDER_BLOCKING_CONFIG_ITEM = "blocking";
const char *SENDER_QUEUE_LENGTH_ITEM = "queue_length";


/**
 * Config of a remote service send rule
 */
struct SendRule
{
    std::string topicLocal;
    std::string topicRemote;
    bool isBlocking = false;
    size_t queueLength = 1UL;
};

/**
 * Config of a remote service receive rule
 */
struct ReceiveRule
{
    std::string topicRemote;
    std::string topicLocal;
};

/**
 * Configuration of a Remote service instance
 */
struct RemoteServiceInstanceConfig
{
    std::string sendConnection;
    std::string receiveConnection;
    std::vector<SendRule> sendRules;
    std::vector<ReceiveRule> receiveRules;
    std::chrono::milliseconds sendTimeout = std::chrono::milliseconds(100);
    std::chrono::milliseconds artificialJitter = std::chrono::milliseconds(0);
};

/**
 * Decode RemoteService send rules
 */
std::vector<SendRule> decodeSendRules(const Json::Value &config)
{
    // check that config is an array of rules
    if (!config.isArray())
    {
        throw Json::RuntimeError(SEND_RULES_CONFIG_ITEM + std::string(" is not an array"));
    }

    // loop over all array entries and decode into rules
    std::vector<SendRule> rules;
    for (const auto &ruleJson: config)
    {
        SendRule rule;

        // get topics
        rule.topicLocal = ruleJson[TOPIC_LOCAL_CONFIG_ITEM].asString();
        rule.topicRemote = ruleJson[TOPIC_REMOTE_CONFIG_ITEM].asString();

        // error, if both topics empty
        if (rule.topicRemote.empty() && rule.topicLocal.empty())
        {
            throw Json::RuntimeError(SEND_RULES_CONFIG_ITEM + std::string(": ") +
                                     "Both topics not specified or empty");
        }

        // if one of the topics is empty, set it equal to other topic
        if (rule.topicRemote.empty())
        {
            rule.topicRemote = rule.topicLocal;
        }
        else if (rule.topicLocal.empty())
        {
            rule.topicLocal = rule.topicRemote;
        }

        // get blocking flag (or use default false)
        if (ruleJson.isMember(SENDER_BLOCKING_CONFIG_ITEM))
        {
            if (!ruleJson[SENDER_BLOCKING_CONFIG_ITEM].isBool())
            {
                throw Json::RuntimeError(SEND_RULES_CONFIG_ITEM +
                                         std::string(": '") +
                                         SENDER_BLOCKING_CONFIG_ITEM +
                                         std::string("' is not boolean"));
            }
            rule.isBlocking = ruleJson[SENDER_BLOCKING_CONFIG_ITEM].asBool();
        }

        // get queue length (or use default 0)
        if (ruleJson.isMember(SENDER_QUEUE_LENGTH_ITEM))
        {
            if (!ruleJson[SENDER_QUEUE_LENGTH_ITEM].isUInt())
            {
                throw Json::RuntimeError(SEND_RULES_CONFIG_ITEM +
                                         std::string(": '") +
                                         SENDER_QUEUE_LENGTH_ITEM +
                                         std::string("' is not a non-negative integer"));
            }
            rule.queueLength = ruleJson[SENDER_QUEUE_LENGTH_ITEM].asUInt();
        }

        rules.push_back(rule);
    }

    return rules;
}

/**
 * Decode RemoteService receive rules
 */
std::vector<ReceiveRule> decodeReceiveRules(const Json::Value &config)
{
    // check that config is an array of rules
    if (!config.isArray())
    {
        throw Json::RuntimeError(RECEIVE_RULES_CONFIG_ITEM + std::string(" is not an array"));
    }

    // loop over all array entries and decode into rules
    std::vector<ReceiveRule> rules;
    for (const auto &ruleJson: config)
    {
        ReceiveRule rule;

        // get topics
        rule.topicLocal = ruleJson[TOPIC_LOCAL_CONFIG_ITEM].asString();
        rule.topicRemote = ruleJson[TOPIC_REMOTE_CONFIG_ITEM].asString();

        // error, if both topics empty
        if (rule.topicRemote.empty() && rule.topicLocal.empty())
        {
            throw Json::RuntimeError(RECEIVE_RULES_CONFIG_ITEM + std::string(": ") +
                                     "Both topics not specified or empty");
        }

        // if one of the topics is empty, set it equal to other topic
        if (rule.topicRemote.empty())
        {
            rule.topicRemote = rule.topicLocal;
        }
        else if (rule.topicLocal.empty())
        {
            rule.topicLocal = rule.topicRemote;
        }

        rules.push_back(rule);
    }

    return rules;
}

/**
 * Decode JSON config node of a single RemoteService instance
 */
RemoteServiceInstanceConfig decodeInstanceConfig(const Json::Value &config)
{
    RemoteServiceInstanceConfig decodedConfig;

    decodedConfig.sendConnection = config[SEND_CONNECTION_CONFIG_ITEM].asString();
    decodedConfig.receiveConnection = config[RECEIVE_CONNECTION_CONFIG_ITEM].asString();
    decodedConfig.sendRules = decodeSendRules(config[SEND_RULES_CONFIG_ITEM]);
    decodedConfig.receiveRules = decodeReceiveRules(config[RECEIVE_RULES_CONFIG_ITEM]);
    if(config.isMember(SENDER_TIMEOUT))
    {
        decodedConfig.sendTimeout = std::chrono::milliseconds(config[SENDER_TIMEOUT].asUInt());
    }
    if(config.isMember(SENDER_ARTIFICIAL_JITTER))
    {
        decodedConfig.artificialJitter = 
            std::chrono::milliseconds(config[SENDER_ARTIFICIAL_JITTER].asUInt());
    }
    return decodedConfig;
};

} // anonymous namespace;


RemoteServiceConfigurator::RemoteServiceConfigurator(ValueStore &valueStore,
                                                     std::shared_ptr<ShmemKeeper> shmemKeeper,
                                                     std::shared_ptr<ShmemClient> shmemClient)
        : fValueStore(valueStore)
        , fShmemKeeper(std::move(shmemKeeper))
        , fShmemClient(std::move(shmemClient))
{
}

std::map<std::string, std::shared_ptr<mcf::remote::RemoteService>>
RemoteServiceConfigurator::configureFromJSONNode(const Json::Value &config)
{
    std::map<std::string, std::shared_ptr<mcf::remote::RemoteService>> instances;

    try
    {
        // loop over all member nodes.
        for (const auto& name: config.getMemberNames())
        {
            // error, if same instance already configured
            if (instances.find(name) != instances.end())
            {
                throw Json::RuntimeError("instance '" + name + "' configured twice");
            }

            // get instance configuration and check that it is an object
            const Json::Value &cfgNode = config[name];
            if (!cfgNode.isObject())
            {
                throw Json::RuntimeError("Config of instance '" + name + "' is not an object");
            }

            // decode instance configuration
            RemoteServiceInstanceConfig instanceConfig;
            try
            {
                instanceConfig = decodeInstanceConfig(cfgNode);
            }
            catch (const Json::Exception &e)
            {
                throw Json::RuntimeError("Instance '" + name + "': " + e.what());
            }

            // create remote service instance
            auto instance = buildZmqRemoteService(instanceConfig.sendConnection,
                                                  instanceConfig.receiveConnection,
                                                  fValueStore,
                                                  fShmemKeeper,
                                                  fShmemClient,
                                                  instanceConfig.sendTimeout,
                                                  instanceConfig.artificialJitter);

            // add send rules
            for (const auto& rule: instanceConfig.sendRules)
            {
                instance->addSendRule(rule.topicLocal, rule.topicRemote, rule.queueLength, rule.isBlocking);
            }

            // add receive rules
            for (const auto& rule: instanceConfig.receiveRules)
            {
                instance->addReceiveRule(rule.topicLocal, rule.topicRemote);
            }

            // store instance
            instances[name] = instance;
        }
    }
    catch (const Json::Exception &e)
    {
        MCF_THROW_RUNTIME(std::string("Cannot configure remote service: ") + e.what());
    }

    return instances;
}

} // namespace remote
} // namespace mcf
