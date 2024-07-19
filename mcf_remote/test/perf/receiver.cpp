/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_remote/RemoteService.h"
#include "mcf_core/ExtMemValue.h"
#include "perf_messages.h"

int main() {

    mcf::ValueStore vs;
    perf_msg::registerValueTypes(vs);

    mcf::ComponentManager cm(vs);
    auto rr = std::make_shared<mcf::remote::RemoteReceiver>(5555, vs);
    cm.registerComponent(rr);

    rr->addReceiveRule<mcf::msg::Timestamp>("/time/10ms");
    rr->addReceiveRule<perf_msg::TestValue>("/test1");
    rr->addReceiveRule<perf_msg::Image>("/test2");

    cm.configure();
    cm.startup();

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));

      for (auto k : vs.getKeys()) {
          std::cout << k << std::endl;
      }

      std::cout << vs.getValueMsgpack("/runtime/RemoteReceiver5555:<run>") << std::endl;
      std::cout << vs.getValue<perf_msg::TestValue>("/test1")->time << std::endl;
      std::cout << vs.getValue<perf_msg::TestValue>("/test1")->str << std::endl;
      std::cout << vs.getValue<perf_msg::TestValue>("/test1")->data.size() << std::endl;
      std::cout << vs.getValue<perf_msg::TestValue>("/test1")->points.size() << std::endl;
      std::cout << vs.getValueMsgpack("/test1") << std::endl;
      std::cout << vs.getValue<perf_msg::Image>("/test2")->width << std::endl;
      std::cout << vs.getValue<perf_msg::Image>("/test2")->height << std::endl;
      std::cout << vs.getValue<perf_msg::Image>("/test2")->extMemSize() << std::endl;
      auto extval = vs.getValue<perf_msg::Image>("/test2");
      if (extval->extMemSize() >= 4) {
          auto extmem = extval->extMemPtr();
          std::cout << extmem[0] << " " << extmem[1] << " " << extmem[2] << " " << extmem[3] << std::endl;
      }
      std::cout << vs.getValue<mcf::msg::Timestamp>("/time/10ms")->ms << std::endl;
    }

    cm.shutdown();

    return 0;
}

