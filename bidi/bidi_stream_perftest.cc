#include <atomic>
#include <thread>
#include <chrono>
#include <deque>

#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "blocks/xterm/vt100.h"
#include "blocks/xterm/progress.h"
#include "bidi_stream.grpc.pb.h"

DEFINE_string(server, "localhost:50051", "The server to connect to.");
DEFINE_uint64(base_id, 1000000001, "The base to increment message IDs from.");
DEFINE_uint64(n, 100'000u, "The number of messages to send.");

using namespace current::vt100;

inline std::chrono::microseconds SteadyNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
}

int main(int argc, char** argv) {
#ifndef NDEBUG
  std::cout << bold << yellow << "WARNING" << reset << ": running a " << bold << red << "DEBUG" << reset
            << " build. Suboptimal for performance testing." << std::endl;
#endif

  ParseDFlags(&argc, &argv);

  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(FLAGS_server, grpc::InsecureChannelCredentials());
  std::unique_ptr<test_bidi_stream::RPCBidiStream::Stub> stub(test_bidi_stream::RPCBidiStream::NewStub(channel));

  struct Handler final : grpc::ClientBidiReactor<test_bidi_stream::Req, test_bidi_stream::Res> {
    Handler(uint64_t total_count, uint64_t index_t0, uint64_t index_t1)
        : total_count_(total_count), index_t0_(index_t0), index_t1_(index_t1) {}
    void ThreadSafeWrite(test_bidi_stream::Req&& req) {
      std::lock_guard lock(mutex_);
      reqs_.push_front(std::move(req));
      if (reqs_.size() == 1u) {
        StartWrite(&reqs_.front());
      }
    }
    void OnWriteDone(bool) override {
      std::lock_guard lock(mutex_);
      reqs_.pop_front();
      if (!reqs_.empty()) {
        StartWrite(&reqs_.front());
      }
    }
    void OnReadDone(bool) override {
      uint64_t const c = ++received_;
      if (c == index_t0_) {
        timestamp_t0_ = SteadyNow();
      } else if (c == index_t1_) {
        timestamp_t1_ = SteadyNow();
      }
      if (c < total_count_) {
        StartRead(&res_);
      }
    }
    std::mutex mutex_;
    test_bidi_stream::Res res_;
    std::deque<test_bidi_stream::Req> reqs_;
    grpc::ClientContext context_;
    std::atomic_uint64_t received_ = std::atomic_uint64_t(0u);
    uint64_t const total_count_;
    uint64_t const index_t0_;
    uint64_t const index_t1_;
    std::chrono::microseconds timestamp_t0_;
    std::chrono::microseconds timestamp_t1_;
  };

  uint64_t const SAVE_n = FLAGS_n;
  uint64_t const index_t0 = static_cast<uint64_t>(SAVE_n * 0.1);
  uint64_t const index_t1 = static_cast<uint64_t>(SAVE_n * 0.9);
  Handler handler(SAVE_n, index_t0, index_t1);

  stub->async()->Go(&handler.context_, &handler);
  handler.StartRead(&handler.res_);
  handler.StartCall();

  std::thread printer([&]() {
    current::ProgressLine report;
    uint64_t value;
    while ((value = handler.received_) < SAVE_n) {
      report << "Received: " << value << " of " << SAVE_n;
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  });

  std::string const input = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  uint64_t id = FLAGS_base_id;
  for (uint64_t iteration = 0u; iteration < SAVE_n; ++iteration) {
    test_bidi_stream::Req req;
    req.set_s(input);
    req.set_id(id++);

    size_t const i = 1u + rand() % (input.length() - 1u);
    size_t const c = rand() % (input.length() - i + 1);
    size_t const n = 1u + (rand() % 50);

    req.set_i(i);
    req.set_n(c);
    req.set_c(n);

    handler.ThreadSafeWrite(std::move(req));
  }

  printer.join();

  {
    uint64_t const n = (index_t1 - index_t0);
    int64_t us = (handler.timestamp_t1_ - handler.timestamp_t0_).count();
    std::cout << "QPS: " << bold << blue << n * 1e6 / us << reset << std::endl;
  }

  handler.context_.TryCancel();
}
