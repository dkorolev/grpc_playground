#include <atomic>
#include <thread>

#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "bidi_stream.grpc.pb.h"

DEFINE_string(server, "localhost:50051", "The server to connect to.");
DEFINE_uint64(base_id, 1000000001, "The base to increment message IDs from.");
DEFINE_uint64(prompt_delay_ms, 50, "Show a prompt if no input within this time interval.");
DEFINE_uint64(end_delay_ms, 500, "Wait for this long after all the input is consumed.");

constexpr static char const* const kPrompt =
    "Line format: word [substring offset = 0] [substring length = 100] [multiplicity = 1]";

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(FLAGS_server, grpc::InsecureChannelCredentials());
  std::unique_ptr<test_bidi_stream::RPCBidiStream::Stub> stub(test_bidi_stream::RPCBidiStream::NewStub(channel));

  grpc::ClientContext context;
  std::shared_ptr<grpc::ClientReaderWriter<test_bidi_stream::Req, test_bidi_stream::Res>> stream(stub->Go(&context));

  uint64_t id = FLAGS_base_id;

  std::atomic_bool terminating(false);
  std::thread reader([&stream, &terminating]() {
    test_bidi_stream::Res res;
    while (!terminating && stream->Read(&res)) {
      std::cout << res.id() << '\t' << res.r() << std::endl;
    }
  });

  std::atomic_bool has_input(false);
  std::thread prompt([&has_input]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_prompt_delay_ms));
    if (!has_input) {
      std::cout << kPrompt << std::endl;
    }
  });

  std::string line;
  while (std::getline(std::cin, line)) {
    has_input = true;
    std::istringstream iss(line);
    std::string s;
    if (iss >> s) {
      test_bidi_stream::Req req;

      req.set_id(id++);

      req.set_s(s);
      req.set_i(0);
      req.set_n(1);
      req.set_c(100);

      int32_t v;
      if (iss >> v) {
        req.set_i(v);
        if (iss >> v) {
          req.set_c(v);
          if (iss >> v) {
            req.set_n(v);
          }
        }
      }
      stream->Write(req);
    } else {
      std::cout << "\x1b[1A" << kPrompt << std::endl;
    }
  }

  terminating = true;
  stream->WritesDone();

  std::this_thread::sleep_for(std::chrono::milliseconds(FLAGS_end_delay_ms));

  reader.join();
  prompt.join();
}
