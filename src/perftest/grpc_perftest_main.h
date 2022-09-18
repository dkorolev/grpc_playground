// NOTE(dkorolev): This is a "dry run", before this code is copied/moved to `C5T/grpcpp/lib`.
//                 PLEASE DO NOT USE THIS CODE, IF ONLY AS A REFERENCE!

#include "grpc_perftest.h"

DECLARE_string(grpc_server);
DEFINE_uint32(intervals, 5, "The number of intervals to run the test for.");
DEFINE_uint32(subintervals, 20, "The number of subintervals per interval.");
DEFINE_double(interval_s, 12, "The maximum duration of each interval, in seconds.");
DEFINE_uint32(j, 32, "The number of threads / channel / virtual users.");

class TestRunner;

template <bool>
int DoMain() {
  std::string const server = FLAGS_grpc_server;
  if (server.empty()) {
    std::cout << "The `--grpc_server` flag should be set, or provided with a default value." << std::endl;
    return -1;
  }

  RunTest<TestRunner>(server,
                      TestConfig()
                          .SetVUs(FLAGS_j)
                          .SetNumberOfIntervals(FLAGS_intervals)
                          .SetNumberOfSubIntervals(FLAGS_subintervals)
                          .SetIntervalDurationInSeconds(FLAGS_interval_s));
  return 0;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  return DoMain<true>();
}
