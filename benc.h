/**
 * @file benc.h
 * @author Stephen Belanger
 * @date March 19, 2024
 * @brief C and C++ single-header benchmarking
 */

#ifndef _INCLUDE_BENC_H_
#define _INCLUDE_BENC_H_

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/mach_time.h>
static mach_timebase_info_data_t timebase;
static bool timebase_initialized = false;
#else
#include <time.h>
#endif

#define NANOS 1
#define MICROS NANOS * 1000
#define MILLIS MICROS * 1000
#define SECONDS MILLIS * 1000

/**
 * Human readable numbers
 *
 * @param out The file to write to.
 * @param number The number to write in human-readable form.
 * @param time Whether the number is a time.
 */
static inline void bench_human_number(FILE* out, float number, bool time) {
  int level_cap = time ? 3 : 4;
  int threshold = 1000;
  int level = 0;

  while (number >= threshold && ++level <= level_cap) {
    number /= threshold;
  }

  if (time) {
    switch (level) {
      case 0: fprintf(out, "%.2fns", number); break;
      case 1: fprintf(out, "%.2fus", number); break;
      case 2: fprintf(out, "%.2fms", number); break;
      default: fprintf(out, "%.2fs", number); break;
    }
  } else {
    switch (level) {
      case 0: fprintf(out, "%.2f", number); break;
      case 1: fprintf(out, "%.2fk", number); break;
      case 2: fprintf(out, "%.2fm", number); break;
      case 3: fprintf(out, "%.2fb", number); break;
      default: fprintf(out, "%.2ft", number); break;
    }
  }
}

/**
 * Dynamic arrays.
 */

/*!
 * A bench_array_t is a dynamic array of pointers.
 *
 * @private
 * @property size The number of pointers in the array.
 * @property capacity The number of pointers the array can hold.
 * @property entries The pointers in the array.
 */
typedef struct bench_array_s {
  size_t size;
  size_t capacity;
  void** entries;
} bench_array_t;

/*!
 * Resize the dynamic array to the given capacity. Can not be smaller than the
 * current size.
 *
 * @private
 * @param array The dynamic array to resize.
 * @param capacity The new capacity of the array.
 * @return Whether the resize was successful.
 */
static inline bool bench_array_resize(bench_array_t* array, size_t capacity) {
  if (capacity < array->size) return false;

  // Create or resize the entries buffer
  void** entries = (array->entries == NULL)
    ? (void**) calloc(capacity, sizeof(void*))
    : (void**) realloc(array->entries, capacity * sizeof(void*));

  if (entries == NULL) return false;

  array->capacity = capacity;
  array->entries = entries;

  return true;
}

/*!
 * Initializes a dynamic array with the given capacity.
 *
 * @private
 * @param capacity The initial capacity of the array.
 * @return Whether the initialization was successful.
 */
static inline bool bench_array_init(bench_array_t* array, size_t capacity) {
  if (array == NULL) return false;
  array->size = 0;
  array->capacity = 0;
  array->entries = NULL;
  return bench_array_resize(array, capacity);
}

/*!
 * Pushes a pointer onto the dynamic array.
 *
 * @private
 * @param array The dynamic array to dispose.
 * @param ptr The pointer to push onto the array.
 * @return Whether the push was successful.
 */
static inline bool bench_array_push(bench_array_t* array, void* ptr) {
  if (array->size == array->capacity) {
    if (!bench_array_resize(array, array->capacity * 2)) return false;
  }

  array->entries[array->size++] = ptr;
  return true;
}

/*!
 * Remove all entries from the dynamic array.
 *
 * @private
 * @param array The dynamic array to clear.
 */
static inline void bench_array_clear(bench_array_t* array) {
  free(array->entries);
  array->size = 0;
}

/**
 * Streaming stats tracking to approximate standard deviation.
 *
 * @property count The number of measurements.
 * @property mean The mean of the measurements.
 * @property d_squared The sum of the squared differences from the mean.
 */
typedef struct bench_stats_s {
  uint32_t count;
  uint64_t total;
  float mean;
  float d_squared;
} bench_stats_t;

/*!
 * Initializes stats object with default values.
 *
 * @param stats The bench_stats_t to initialize.
 */
static inline void bench_stats_init(bench_stats_t* stats) {
  stats->count = 0;
  stats->total = 0;
  stats->mean = 0;
  stats->d_squared = 0;
}

/*!
 * Records a new value to the stats object.
 *
 * @param stats The bench_stats_t to which to record.
 * @param value The value to record.
 */
static inline void bench_stats_push(bench_stats_t* stats, uint64_t value) {
  stats->count++;
  stats->total += value;

  float new_mean = stats->mean + (value - stats->mean) / stats->count;
  float d_squared_increment = (value - new_mean) * (value - stats->mean);

  stats->mean = new_mean;
  stats->d_squared += d_squared_increment;
}

/*!
 * Returns the variance of the stats object.
 *
 * @param stats The bench_stats_t for which to compute variance.
 * @return The variance of the stats object.
 */
static inline float bench_stats_variance(bench_stats_t* stats) {
  return stats->d_squared / stats->count;
}

/*
 * Returns the standard deviation of the stats object.
 *
 * @param stats The bench_stats_t for which to compute standard deviation.
 * @return The standard deviation of the stats object.
 */
static inline float bench_stats_stddev(bench_stats_t* stats) {
  return sqrt(bench_stats_variance(stats));
}

/*!
 * Returns the operations per second of the stats object.
 *
 * @param stats The bench_stats_t for which to compute operations per second.
 * @return The operations per second of the stats object.
 */
static inline float bench_stats_ops_per_sec(bench_stats_t* stats) {
  return ((float) stats->count / (float) stats->total) * SECONDS;
}

/*!
 * Prints the stats object to the given output.
 *
 * @param stats The bench_stats_t to print.
 * @param out The output to which to print.
 */
static inline void bench_stats_print(bench_stats_t* stats, FILE* out) {
  bench_human_number(out, bench_stats_ops_per_sec(stats), false);
  fprintf(out, " i/s (±%.2f%%) (", bench_stats_stddev(stats));
  bench_human_number(out, stats->mean, true);
  fprintf(out, "/i)");
}

/*!
 * A named measurement with stats.
 *
 * @property name The name of the measurement.
 * @property stats The stats of the measurement.
 */
typedef struct bench_measurement_s {
  const char* name;
  bench_stats_t stats;
} bench_measurement_t;

/*!
 * Free the measurement and its name.
 *
 * @param m The bench_measurement_t to free.
 */
static inline void bench_measurement_free(bench_measurement_t* m) {
  free((void*) m->name);
  free(m);
}

// A list of measurements
typedef bench_array_t bench_measurements_t;

/**
 * A bench namespace.
 *
 * @property name The name of the benchmark suite.
 * @property out The target to which output is written.
 * @property indent The indentation level for sub-groups.
 * @property data A free pointer slot to pass data into sub-groups.
 * @property target_time The approximate time in nanoseconds to run each measurement.
 * @property measurements The list of measurements.
 */
typedef struct bench_s {
  const char* name;
  FILE* out;
  int indent;
  void* data;
  uint64_t target_time;
  bench_measurements_t measurements;
} bench_t;

/*!
 * Free the benchmark and its name.
 *
 * @private
 * @param b bench namespace
 */
static inline void bench_free(bench_t* b) {
  free((void*) b->name);
  for (size_t i = 0; i < b->measurements.size; i++) {
    bench_measurement_free((bench_measurement_t*) b->measurements.entries[i]);
  }
  bench_array_clear(&b->measurements);
  free(b);
}

/*!
 * Print indentation level for sub-groups
 *
 * @private
 * @param b bench namespace
 */
static inline void _bench_print_indent(bench_t* b) {
  for (int i = 0; i < b->indent; i++) {
    fprintf(b->out, " ");
  }
}

/**
 * Creates a benchmark writing to the given FILE.
 *
 * ```c
 * bench_t* b = bench_create("name", stdout);
 * // or...
 * bench_t* b = bench_create("name", fopen("out.bench", "w"));
 * ```
 *
 * @param name The name of the benchmark suite
 * @param out FILE to which the benchmark output will be written
 * @param indent The indentation level for sub-groups
 * @param data A free pointer slot to pass data into sub-groups
 * @return bench_t
 */
static inline bench_t* bench_create(const char* name, FILE* out, int indent, void* data, ...) {
  bench_t* b = (bench_t*) malloc(sizeof(bench_t));

  b->name = strdup(name);
  b->out = out;
  b->indent = indent;
  b->data = data;
  b->target_time = SECONDS;

  // Attempt to allocate the bench array
  if (!bench_array_init((bench_measurements_t*) &b->measurements, 16)) {
    bench_free(b);
    return NULL;
  }

  // For top-level suite, print the header
  if (indent == 0) {
    fprintf(out, "benc.h v1.0.0\n");
  }
  _bench_print_indent(b);
  fprintf(b->out, "# %s\n", b->name);

  return b;
}
// Make out parameter optional, defaulting to stdout
#define bench_create(name, out, ...) bench_create(name, out, ##__VA_ARGS__, 0, NULL)

/*!
 * Compare two measurements to sort by operations per second.
 *
 * @private
 * @param a The first measurement cast as `const void*`.
 * @param b The second measurement cast as `const void*`.
 */
static inline int _bench_compare_measurement(const void* a, const void* b) {
  bench_measurement_t* ma = *(bench_measurement_t**) a;
  bench_measurement_t* mb = *(bench_measurement_t**) b;

  float current = bench_stats_ops_per_sec(&ma->stats);
  float next = bench_stats_ops_per_sec(&mb->stats);

  if (current > next) return -1;
  if (current < next) return 1;
  return 0;
}

/**
 * Compare results of the benchmark.
 *
 * ```c
 * bench_t* b = bench_create("bench", stdout);
 * bench_measure(b, "fast", bench_fast);
 * bench_measure(b, "slow", bench_slow);
 * bench_compare(b);
 * ```
 *
 * ```
 * benc.h v1.0.0
 * # bench
 * fast - 26.92m i/s (±49.87%) (34.11ns/i)
 * slow - 3.53m i/s (±165.47%) (278.89ns/i)
 * ```
 *
 * @param b bench namespace
 */
static inline void bench_compare(bench_t* b) {
  bench_array_t* m = &b->measurements;

  // Only show comparison if there's more than one measurement.
  if (m->size > 1) {
    // Sort the order of the measurements by the number of operations per second
    qsort(m->entries, m->size, sizeof(void**), _bench_compare_measurement);

    _bench_print_indent(b);
    fprintf(b->out, "Comparing...\n");

    bench_stats_t* first = NULL;

    for (size_t i = 0; i < m->size; i++) {
      bench_measurement_t* measurement = (bench_measurement_t*) m->entries[i];
      bench_stats_t* stats = &measurement->stats;
      _bench_print_indent(b);
      if (first == NULL) {
        fprintf(b->out, "  - %s (fastest)\n", measurement->name);
        first = stats;
      } else {
        fprintf(b->out, "  - %s", measurement->name);
        fprintf(b->out, " (%.2f%% slower)\n", (stats->mean / first->mean) * 100 - 100);
      }
    }
  }

  bench_free(b);
}

/**
 * Sub-group function signature
 *
 * @param b bench namespace
 */
typedef void (*bench_group_fn)(bench_t* b);

/**
 * Add a named sub-group
 *
 * ```c
 * void sub_group(bench_t* b) {
 *   bench_measure(b, "fast", bench_fast);
 *   bench_measure(b, "slow", bench_slow);
 * }
 *
 * bench_t* b = bench_create("bench", stdout);
 * bench_group(b, "sub-group", sub_group);
 * bench_compare(b);
 * ```
 *
 * ```
 * benc.h v1.0.0
 * # bench
 *   # sub-group
 *   fast - 26.92m i/s (±49.87%) (34.11ns/i)
 *   slow - 3.53m i/s (±165.47%) (278.89ns/i)
 * ```
 *
 * @param b bench namespace
 * @param name Group name
 * @param fn Group function
 * @param ptr Optional pointer to attach to bench_t given to group function
 */
static inline void bench_group(bench_t* b, const char *name, bench_group_fn fn, void *ptr, ...) {
  bench_t* b2 = bench_create(name, b->out, b->indent + 2, ptr);
  fn(b2);
  bench_compare(b2);
}

// Hack to make ptr optional
#define bench_group(t, name, fn, ...) \
  bench_group(t, name, fn, ##__VA_ARGS__, NULL)

/*!
 * Initialize a new bench measurement.
 *
 * @private
 * @param m The measurement to initialize.
 * @param name The name of the measurement.
 */
static inline void bench_measurement_init(
  bench_measurement_t* m,
  const char* name
) {
  m->name = strdup(name);
  bench_stats_init(&m->stats);
}

/**
 * Measure function signature
 *
 * @param data Optional pointer passed through from `bench_measure`
 */
typedef void (*bench_measure_fn)(void* data);

/*!
 * Get high-resolution monotonic timestamp.
 *
 * @private
 * @todo Support Windows
 * @return Monotonic timestamp in nanoseconds.
 */
static inline uint64_t bench_now() {
#ifdef __APPLE__
  return mach_continuous_time() * timebase.numer / timebase.denom;
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * SECONDS + t.tv_nsec;
#endif
}

/**
 * Measure performance of the given function.
 *
 * ```c
 * bench_t* b = bench_create("bench", stdout);
 * bench_measure(b, "fast", bench_fast);
 * bench_measure(b, "slow", bench_slow);
 * bench_compare(b);
 * ```
 *
 * ```
 * benc.h v1.0.0
 * # bench
 * fast - 26.92m i/s (±49.87%) (34.11ns/i)
 * slow - 3.53m i/s (±165.47%) (278.89ns/i)
 * ```
 *
 * @param b bench namespace
 * @param name Measurement name
 * @param fn Measurement function
 * @param data Optional pointer passed to `fn`
 */
static inline int bench_measure(bench_t* b, const char* name, bench_measure_fn fn, void* data, ...) {
  if (!timebase_initialized) {
    mach_timebase_info(&timebase);
    timebase_initialized = true;
  }

  _bench_print_indent(b);
  fprintf(b->out, "%s - ", name);
  fflush(b->out);

  // Create a new measurement
  bench_measurement_t* m = (bench_measurement_t*) malloc(sizeof(bench_measurement_t));
  bench_measurement_init(m, name);

  // Add measurement to the current suite
  bench_array_push(&b->measurements, m);

  uint64_t start = bench_now();
  uint64_t end;

  while (m->stats.total < b->target_time) {
    start = bench_now();
    fn(data);
    end = bench_now();
    bench_stats_push(&m->stats, end - start);
  }

  bench_stats_print(&m->stats, b->out);
  fprintf(b->out, "\n");
  return 0;
}

// Hack to make ptr optional
#define bench_measure(b, name, fn, ...) \
  bench_measure(b, name, fn, ##__VA_ARGS__, NULL)

/*!
 * C++11 API
 */
#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)

#include <functional>
#include <string>

namespace bench {

class Group {
  private:

  bench_t* bench;

  public:

  /**
   * Wrap an existing bench namespace
   *
   * @param b bench namespace
   */
  Group(bench_t* b) : bench(b) {}

  /**
   * Construct a new bench namespace
   *
   * @param name Group name
   * @param out Target stream to which to write results. Defaults to stdout.
   */
  Group(std::string name, FILE* out = stdout)
    : bench(bench_create(name.c_str(), out, 0, NULL)) {}

  /**
   * Do comparison when the group is destroyed.
   *
   * ```cpp
   * bench::Group b("bench");
   * b.measure("fast", [](){});
   * b.measure("slow", [](){});
   * ```
   */
  ~Group() {
    if (bench->indent == 0) {
      bench_compare(bench);
    }
  }

  /**
   * Function for which to measure performance.
   */
  using MeasureFn = std::function<void()>;

  /**
   * Measure performance of the given function.
   *
   * ```cpp
   * bench::Group b("bench");
   * b.measure("fast", [](){});
   * b.measure("slow", [](){});
   * ```
   *
   * @param name Measurement name
   * @param fn Measurement function
   */
  void measure(std::string name, MeasureFn fn) {
    bench_measure(bench, name.c_str(), [](void* data) {
      MeasureFn* fn = (MeasureFn*) data;
      (*fn)();
    }, &fn);
  }

  /**
   * Function to group a collection of measurements.
   *
   * @param group The Group object
   */
  using GroupFn = std::function<void(Group*)>;

  /**
   * Group a collection of measurements.
   *
   * ```cpp
   * bench::Group b("bench");
   * b.group("group", [](bench::Group* g){
   *   g->measure("fast", [](){});
   *   g->measure("slow", [](){});
   * });
   * ```
   *
   * @param name Group name
   * @param fn Group function
   */
  void group(const char* name, GroupFn fn) {
    struct GroupData {
      GroupFn fn;
    };

    GroupData* group_data = new GroupData { fn };

    bench_group(bench, name, [](bench_t* b) {
      GroupData* data = (GroupData*) b->data;
      Group g(b);
      data->fn(&g);
    }, group_data);
  }
};

} // namespace bench

#endif // __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)

#endif // _INCLUDE_BENC_H_
