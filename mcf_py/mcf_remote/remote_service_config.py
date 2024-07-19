"""
Copyright (c) 2024 Accenture
"""

from mcf_remote.remote_service import RemoteService
from mcf_remote.zmq_msgpack_comm import ZmqMsgPackReceiver, ZmqMsgPackSender


REMOTE_SERVICE_TIMEOUT_MS = 3000


def configure_remote_services(services_cfg, value_store, component_manager, receive_types):
    """
    Helper creating and registering remote services as defined in a config dictionary
    (usually obtained from the flux system config file)

    :param services_cfg:        the contents of the 'RemoteServices' confuguration item in the flux system config file
    :param value_store:         the value store which the remote services will be connected to
    :param component_manager:   the component manager the remote services will be registered with
    :param receive_types:       A list of value type classes containing the types the remote service shall receive
    """

    # loop over the requested services
    for name, cfg in services_cfg.items():
        remote_send_conn = cfg["sendConnection"]
        remote_recv_conn = cfg["receiveConnection"]
        send_rules = cfg["sendRules"]
        receive_rules = cfg["receiveRules"]

        sender = ZmqMsgPackSender(remote_send_conn, REMOTE_SERVICE_TIMEOUT_MS)
        receiver = ZmqMsgPackReceiver(remote_recv_conn, REMOTE_SERVICE_TIMEOUT_MS, receive_types)
        remote_service = RemoteService(value_store, sender, receiver)

        for r in send_rules:

            # get local and remote topic, use same topic for both, if one of them is None
            topic_local = r.get("topic_local", None)
            topic_remote = r.get("topic_remote", topic_local)
            topic_local = r.get("topic_local", topic_remote)

            # make sure we have got a topic
            assert topic_local is not None, "Remote service {name}: found send rule with no topic set"

            queue_len = r["queue_length"]
            blocking = r["blocking"]
            remote_service.add_send_rule(topic_local, topic_remote, queue_len, blocking)

        for r in receive_rules:
            # get local and remote topic, use same topic for both, if one of them is None
            topic_local = r.get("topic_local", None)
            topic_remote = r.get("topic_remote", topic_local)
            topic_local = r.get("topic_local", topic_remote)

            # make sure we have got a topic
            assert topic_local is not None, "Remote service {name}: found receive rule with no topic set"

            remote_service.add_receive_rule(topic_local, topic_remote)

        component_manager.register_component(remote_service, name)
