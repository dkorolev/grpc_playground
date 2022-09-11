#include <thread>

#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "modulo_schema.grpc.pb.h"

DEFINE_string(server, "", "The server to connect to, for example, `localhost:5001`.");
DEFINE_uint64(a, 10, "The numerator.");
DEFINE_uint64(b, 3, "The denominator.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (FLAGS_server.empty()) {
    std::cout << "Please set `--server`, for example, `--server localhost:5001`." << std::endl;
    return -1;
  }

  std::cout << "Computing `" << FLAGS_a << " mod " << FLAGS_b << "`." << std::endl;

  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(FLAGS_server, grpc::InsecureChannelCredentials());
  std::cout << "Channel created." << std::endl;

  std::unique_ptr<service_compute_modulo::RPC::Stub> stub(service_compute_modulo::RPC::NewStub(channel));
  std::cout << "Stub created." << std::endl;

  service_compute_modulo::Req req;
  req.set_a(FLAGS_a);
  req.set_b(FLAGS_b);

  service_compute_modulo::Res res;

  grpc::ClientContext context;
  grpc::Status const status = stub->ComputeMod(&context, req, &res);

  if (status.ok()) {
    std::cout << "Modulo: " << res.c() << '.' << std::endl;
  } else {
    std::cout
        << "Error: code " << static_cast<int>(status.error_code())
        << ", message `" << status.error_message() << "`." << std::endl;
  }
}
