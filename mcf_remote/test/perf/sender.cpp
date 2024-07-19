/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/TimeService.h"
#include "mcf_core/ValueRecorder.h"
#include "mcf_remote/RemoteService.h"
#include "mcf_remote/RemoteControl.h"
#include "perf_messages.h"

class TestComponent : public mcf::Component {

public:
    TestComponent() :
        mcf::Component("TestComponent"),
        fTimeReceiver(*this, "TimeReceiver"),
        fTest1Sender(*this, "Test1Sender"),
        fTest2Sender(*this, "Test2Sender")
    {
        fTimeReceiver.registerHandler(std::bind(&TestComponent::tick, this));
    }

    void configure(mcf::IComponentConfig& config) {
        config.registerPort(fTimeReceiver, "/time/10ms");
        config.registerPort(fTest1Sender, "/test1");
        config.registerPort(fTest2Sender, "/test2");
    }

    void tick() {
      /*
        perf_msg::TestValue t;
        t.time = valueStore.getValue<mcf::msg::Timestamp>("/time/10ms")->ms;
        t.data.resize(5000000);
        valueStore.setValue("/test1", t);
        */

        std::unique_ptr<perf_msg::Image> i(new perf_msg::Image());
        i->extMemInit(5000000);
        i->extMemPtr()[0] = 't';
        i->extMemPtr()[1] = 'e';
        i->extMemPtr()[2] = 's';
        i->extMemPtr()[3] = 't';
        i->width = 320;
        i->height = 256;
        fTest2Sender.setValue(std::move(i));

        perf_msg::TestValue tv;
        tv.time = 12345;
        tv.str = "hello";
        tv.data = {1, 2, 3};
        perf_msg::Point p;
        p.x = 1;
        p.y = 2;
        tv.points.push_back(p);
        fTest1Sender.setValue(tv);

    }

    mcf::ReceiverPort<mcf::msg::Timestamp> fTimeReceiver;
    mcf::SenderPort<perf_msg::TestValue> fTest1Sender;
    mcf::SenderPort<perf_msg::Image> fTest2Sender;

};

int main() {

    mcf::ValueStore vs;
    perf_msg::registerValueTypes(vs);

    mcf::ValueRecorder valueRecorder(vs);
    valueRecorder.start("record_sender.bin");
    //valueRecorder.enableExtMemSerialization("/test2");

    mcf::ComponentManager cm(vs);

    auto rc = std::make_shared<mcf::remote::RemoteControl>(6666, cm, vs);

    auto rs = std::make_shared<mcf::remote::RemoteSender>(vs);
    auto ts = std::make_shared<mcf::TimeService>();
    auto tc = std::make_shared<TestComponent>();

    cm.registerComponent(rs);
    cm.registerComponent(ts);
    cm.registerComponent(tc);
    cm.registerComponent(rc);

    rs->addSendRule<mcf::msg::Timestamp>("/time/10ms", "tcp://localhost:5555");
    rs->addSendRule<perf_msg::TestValue>("/test1", "tcp://localhost:5555");
    rs->addSendRule<perf_msg::Image>("/test2", "tcp://localhost:5555");

    cm.configure();
    cm.startup();

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      for (auto k : vs.getKeys()) {
          std::cout << k << std::endl;
      }

      std::cout << vs.getValueMsgpack("/runtime/RemoteSender:/test1") << std::endl;
    }

    cm.shutdown();

    valueRecorder.stop();

    return 0;
}
