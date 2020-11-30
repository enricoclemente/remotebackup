#include "Client.h"
#include <iostream>

int main() {
  Client pippo("127.0.0.1", "8888");

  RBRequest test;

  test.set_type("auth");

  std::cout << "Running test request" << std::endl;

  RBResponse res = pippo.run(test);

  std::cout << "RES: " << res.type() << std::endl;

  return 0;  

}