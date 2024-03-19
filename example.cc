#include "benc.h"

using bench::Group;

int fibo(int n) {
  if (n < 2) {
    return n;
  }
  return fibo(n - 1) + fibo(n - 2);
}

int main() {
  Group b("bench");

  b.group("publish", [](Group* b) {
    b->measure("fast", []() { fibo(5); });
    b->measure("slow", []() { fibo(10); });
  });

  return 0;
}
