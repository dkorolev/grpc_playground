// NOTE(dkorolev): This is a "dry run", before this code is copied/moved to `C5T/grpcpp/lib`.
//                 PLEASE DO NOT USE THIS CODE, IF ONLY AS A REFERENCE!

#pragma once

#include <random>
#include <thread>

#include "blocks/xterm/vt100.h"
#include "bricks/dflags/dflags.h"
#include "bricks/time/chrono.h"
#include "typesystem/serialization/json.h"

#include "grpcpp/grpcpp.h"

using namespace current::vt100;

struct TestConfig final {
  uint32_t intervals = 5u;
  uint32_t subintervals = 20u;
  double interval_s = 12.0;
  uint32_t vus = 32u;
  size_t shuffle_random_seed = std::chrono::system_clock::now().time_since_epoch().count();

  TestConfig& SetVUs(uint32_t param) {
    vus = param;
    return *this;
  }
  TestConfig& SetNumberOfIntervals(uint32_t param) {
    intervals = param;
    return *this;
  }
  TestConfig& SetNumberOfSubIntervals(uint32_t param) {
    subintervals = param;
    return *this;
  }
  TestConfig& SetIntervalDurationInSeconds(double param) {
    interval_s = param;
    return *this;
  }
};

struct NotEnoughDigitsToRoundException final {};

// TODO(dkorolev): Clean this up, test it, move into `current`.
enum class RoundType : int { Down, Up };
inline void RoundOneChar(std::string& s, RoundType type = RoundType::Down) {
  size_t i = s.length();
  while (i && s[i - 1] == '0' || s[i - 1] == '.') {
    --i;
  }
  if (!i) {
    throw NotEnoughDigitsToRoundException();
  }
  --i;
  if (!i && type == RoundType::Down) {
    throw NotEnoughDigitsToRoundException();
  }

  s[i] = '0';

  if (type == RoundType::Up) {
    if (i) {
      bool carry = true;
      while (carry && (--i)) {
        if (s[i] >= '0' && s[i] <= '8') {
          ++s[i];
          carry = false;
        } else if (s[i] == '9') {
          s[i] = '0';
        }
      }
    }
    if (!i) {
      s = "1" + s;
    }
  }

  if (s.find('.') != std::string::npos) {
    while (s.length() && s.back() == '0') {
      s.resize(s.length() - 1u);
    }
    if (s.back() == '.') {
      s.resize(s.length() - 1u);
    }
  }
}

template <class F>
std::string Round(double x, F&& predicate, RoundType type = RoundType::Down) {
  std::string s = current::ToString(x);
  std::string result = s;
  do {
    try {
      RoundOneChar(s, type);
    } catch (NotEnoughDigitsToRoundException const&) {
      break;
    }
    if (!predicate(current::FromString<double>(s))) {
      break;
    }
    result = s;
  } while (true);
  return result;
}

template <class TEST>
void RunTest(std::string const& server_path, TestConfig const config = TestConfig()) {
  struct Entry final {
    typename TEST::GRPCRequest req;
    typename TEST::GRPCResponse res;
    typename TEST::GoldenResponse golden;
    int grpc_status;
    double grpc_ms;

    Entry() = default;
    Entry(typename TEST::GRPCRequest req, typename TEST::GoldenResponse golden)
        : req(std::move(req)), golden(std::move(golden)) {}
  };

  std::cout << "Preparing test data ..." << std::flush;

  std::vector<Entry> entries;
  TEST::GenerateData([&](typename TEST::GRPCRequest req, typename TEST::GoldenResponse golden) {
    entries.emplace_back(std::move(req), std::move(golden));
  });

  auto const PrintErrorCodesHistogramIfExists = [&entries]() {
    std::map<int, uint64_t> error_codes_histogram;
    for (auto const& e : entries) {
      int const v = e.grpc_status;
      if (v && v != -1) {
        // `v == -1` stands for "unused", and `v == 0` is gRPC OK.
        ++error_codes_histogram[v];
      }
    }
    if (!error_codes_histogram.empty()) {
      std::cout << "gRPC error codes histogram: " << JSON(error_codes_histogram) << std::endl;
      if (error_codes_histogram.count(14)) {
        std::cout << "NOTE: `14` stands for 'server unavailable', check if it is running and available." << std::endl;
      }
      return true;
    } else {
      return false;
    }
  };

  std::cout << "\b\b\b\b: " << entries.size() << " datapoints available." << std::endl;
  std::cout << "Running on " << config.vus << " VUs, " << config.intervals << " intervals of " << config.interval_s
            << " seconds, " << config.subintervals << " subintervals each." << std::endl;
  std::cout << "In (A ± B), A stands for the mean, and B is 1.96 times standard deviation." << std::endl;

  for (uint32_t interval = 0u; interval < config.intervals; ++interval) {
    for (auto& e : entries) {
      e.grpc_status = -1;
      e.grpc_ms = -1;
    }
    std::shuffle(std::begin(entries), std::end(entries), std::default_random_engine(config.shuffle_random_seed));

    std::atomic_size_t index_current(static_cast<size_t>(-1));
    size_t const index_last(entries.size());
    std::atomic_bool officially_done(false);
    std::atomic_bool officially_done_error_printed(false);

    std::atomic_size_t total_requests(0);
    std::atomic_size_t total_errors(0);
    // NOTE(dkorolev): Only considering successful requests for pass/fail now.
    std::atomic_size_t total_pass(0);
    std::atomic_size_t total_fail(0);

    struct ThreadData final {
      std::shared_ptr<grpc::Channel> channel;
      std::unique_ptr<typename TEST::GRPCService::Stub> stub;
    };

    std::vector<ThreadData> thread_data(config.vus);

    auto const configure = [&](size_t thread_index) {
      ThreadData& td = thread_data[thread_index];
      td.channel = grpc::CreateChannel(server_path, grpc::InsecureChannelCredentials());
      td.stub = TEST::GRPCService::NewStub(td.channel);
    };
    auto const t0 = current::time::Now();
    std::vector<std::thread> thread_configure;
    for (size_t i = 0u; i < config.vus; ++i) {
      thread_configure.emplace_back(configure, i);
    }
    for (auto& thread : thread_configure) {
      thread.join();
    }
    auto const t1 = current::time::Now();
    std::cout << std::endl
              << bold << blue << "Interval " << interval + 1u << reset << ':' << std::endl
              << "  " << (t1 - t0).count() * 1e-3 << "ms to open " << config.vus << " channels." << std::endl;

    std::atomic_uint64_t sum_dt_us(0ull);
    std::atomic_uint64_t sum_dt_us_squared(0ull);

    auto const run = [&](size_t thread_index) {
      ThreadData& td = thread_data[thread_index];
      td.channel = grpc::CreateChannel(server_path, grpc::InsecureChannelCredentials());
      td.stub = TEST::GRPCService::NewStub(td.channel);

      auto const run_request = [&td, &entries](size_t i) -> grpc::Status {
        grpc::ClientContext context;
        return TEST::Run(*td.stub, &context, entries[i].req, &entries[i].res);
      };

      while (true) {
        size_t const i = (++index_current);
        if (i < entries.size()) {
          ++total_requests;
          auto const t0 = std::chrono::steady_clock::now();
          grpc::Status status = run_request(i);
          auto const t1 = std::chrono::steady_clock::now();
          uint64_t const dt_us =
              static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());

          if (!status.ok()) {
            // TODO(dkorolev): Measure QPS and latency separately for successes and failures.
            ++total_errors;
            entries[i].grpc_status = static_cast<int>(status.error_code());
          } else {
            entries[i].grpc_status = 0;
            entries[i].grpc_ms = 1e-3 * dt_us;

            if (TEST::Validate(entries[i].res, entries[i].golden)) {
              ++total_pass;
            } else {
              ++total_fail;
            }
          }
        } else {
          if (!officially_done) {
            std::atomic_bool ensure_exacly_one_error_message(true);
            if (!officially_done_error_printed.exchange(ensure_exacly_one_error_message)) {
              std::cout << '\n';
              if (PrintErrorCodesHistogramIfExists()) {
                std::cout << "\nThe server is not running, or there are not";
              } else {
                std::cout << "Not";
              }
              std::cout << " enough pre-generated datapoints to run the test.\n";
              std::cout << "Make sure the server is running, and pre-generate more points,"
                        << " and/or make the test shorter." << std::endl;
              std::exit(-1);
            }
          }
          break;
        }
      }
    };

    std::vector<std::thread> thread_run;
    for (size_t i = 0u; i < config.vus; ++i) {
      thread_run.emplace_back(run, i);
    }

    double qps_sum = 0.0;
    double qps_sum_squares = 0.0;

    std::thread([&]() {
      size_t save_requests = 0u;
      auto& progress = std::cout;
      auto const subinterval_ms = static_cast<int64_t>(1e3 * config.interval_s / config.subintervals);
      for (uint32_t subinterval = 0u; subinterval < config.subintervals; ++subinterval) {
        std::this_thread::sleep_for(std::chrono::milliseconds(subinterval_ms));
        size_t const save_requests_at_interval_end = total_requests;
        size_t const subinterval_queries = save_requests_at_interval_end - save_requests;
        double const qps = 1e3 * subinterval_queries / subinterval_ms;
        progress << "  Subinterval " << subinterval + 1u << ", QPS: " << green << qps << reset << '.' << std::endl;

        save_requests = save_requests_at_interval_end;
        qps_sum += qps;
        qps_sum_squares += qps * qps;
      }
      officially_done = true;
      index_current = entries.size();
    }).join();

    for (auto& thread : thread_run) {
      thread.join();
    }

    double const qps_mean = qps_sum / config.subintervals;
    double const qps_stddev = sqrt((qps_sum_squares / config.subintervals) - (qps_mean * qps_mean));
    double const qps_95p = qps_stddev * 1.96;
    std::string const qps_95p_as_string = Round(
        qps_95p,
        [qps_95p](double x) { return x && qps_95p && (std::max(x, qps_95p) / std::min(x, qps_95p)) < 1.037; },
        RoundType::Up);
    double const qps_95p_rounded = current::FromString<double>(qps_95p_as_string);
    std::string const qps_mean_as_string = Round(
        qps_mean,
        [qps_mean, qps_95p_rounded](double x) { return (qps_mean - x) / qps_95p_rounded < 1.0037; },
        RoundType::Down);
    std::cout << "  QPS over " << config.subintervals << " subintervals: " << green << bold << qps_mean_as_string
              << " ± " << qps_95p_as_string << reset << '.' << std::endl;

    std::vector<double> latencies;
    for (auto const& e : entries) {
      if (e.grpc_ms >= 0.0) {
        latencies.push_back(e.grpc_ms);
      }
    }

    std::ostringstream oss;
    bool first_latency = true;
    std::vector<double> const ps({25, 50, 75, 90, 95, 99, 99.9});
    for (double const p : ps) {
      size_t const z = static_cast<size_t>(latencies.size() * 0.01 * p);
      if (z >= 3u && latencies.size() - z >= 3u) {
        std::nth_element(std::begin(latencies), std::begin(latencies) + z, std::end(latencies));
        if (first_latency) {
          first_latency = false;
          oss << "  Latency percentiles over all subintervals:\n";
        }
        oss << "    P" << p << ":\t" << bold << cyan << current::strings::RoundDoubleToString(latencies[z], 2) << "ms"
            << reset << "\n";
      }
    }
    if (!first_latency) {
      std::cout << oss.str() << std::flush;
    }

    PrintErrorCodesHistogramIfExists();

    if (total_errors) {
      std::cout << "Has gRPC errors: " << total_errors << " of " << total_requests << std::endl;
    }

    if (total_fail) {
      std::cout << "Has test errors: " << total_fail << " of " << (total_pass + total_fail) << std::endl;
    }
  }
}
