#include <atomic>
#include <thread>
#include <chrono>

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

  grpc::ClientContext context;
  std::shared_ptr<grpc::ClientReaderWriter<test_bidi_stream::Req, test_bidi_stream::Res>> stream(stub->Go(&context));

  uint64_t const SAVE_n = FLAGS_n;

  uint64_t const index_t0 = static_cast<uint64_t>(SAVE_n * 0.1);
  uint64_t const index_t1 = static_cast<uint64_t>(SAVE_n * 0.9);
  std::chrono::microseconds timestamp_t0;
  std::chrono::microseconds timestamp_t1;

  std::atomic_uint64_t received(0u);
  std::thread reader([&stream, &received, &index_t0, &index_t1, &timestamp_t0, &timestamp_t1]() {
    test_bidi_stream::Res res;
    while ((received < FLAGS_n) && stream->Read(&res)) {
      uint64_t const c = ++received;
      if (c == index_t0) {
        timestamp_t0 = SteadyNow();
      } else if (c == index_t1) {
        timestamp_t1 = SteadyNow();
      }
    }
  });

  std::thread printer([&]() {
    current::ProgressLine report;
    uint64_t value;
    while ((value = received) < SAVE_n) {
      report << "Received: " << value << " of " << SAVE_n;
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
  });

  std::string const input = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  test_bidi_stream::Req req;
  req.set_s(input);

  uint64_t id = FLAGS_base_id;
  for (uint64_t iteration = 0u; iteration < SAVE_n; ++iteration) {
    req.set_id(id++);

    size_t const i = 1u + rand() % (input.length() - 1u);
    size_t const c = rand() % (input.length() - i + 1);
    size_t const n = 1u + (rand() % 50);

    req.set_i(i);
    req.set_c(c);
    req.set_n(n);

    stream->Write(req);
  }

  stream->WritesDone();

  reader.join();
  printer.join();

  {
    uint64_t const n = (index_t1 - index_t0);
    int64_t us = (timestamp_t1 - timestamp_t0).count();
    std::cout << "QPS: " << bold << blue << n * 1e6 / us << reset << std::endl;
  }
}
