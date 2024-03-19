#include "benc.h"

int fibo(int n) {
  if (n < 2) {
    return n;
  }
  return fibo(n - 1) + fibo(n - 2);
}

void bench_fib_to_n(void* data) {
  fibo((int) data);
}

void bench_publish_suite(bench_t* b) {
  bench_measure(b, "fast", bench_fib_to_n, (void*) 5);
  bench_measure(b, "slow", bench_fib_to_n, (void*) 10);
}

int main() {
  bench_t* b = bench_create("bench", stdout);

  bench_group(b, "publish", bench_publish_suite);

  bench_compare(b);
  return 0;
}
