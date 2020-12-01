#include "Client.h"
#include <iostream>

int main() {
  Client pippo("127.0.0.1", "8888");

  RBRequest test;

  test.set_type(RBMsgType::AUTH);
  test.set_final(true);

  std::cout << "Running test request" << std::endl;

  try {
    RBResponse res = pippo.run(test);
    std::cout << "RES: " << res.type() << std::endl;
    if (res.error().size()) {
      RBLog(res.error());
    }
  } catch (RBException &e) {
    excHandler(e);
  }


  return 0;  

}