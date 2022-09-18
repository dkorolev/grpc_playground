#include <thread>

#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "modulo_schema.grpc.pb.h"

DEFINE_uint16(server_port, 5001, "Starts the gRPC service on this port.");

struct ServiceMulImpl final : service_compute_modulo::RPC::Service {
  grpc::Status ComputeMod(grpc::ServerContext* context,
                          service_compute_modulo::Req const* req,
                          service_compute_modulo::Res* res) override {
    if (req->b()) {
      res->set_c(req->a() % req->b());
      return grpc::Status::OK;
    } else {
      return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Division by zero.");
    }
  }
};

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (!FLAGS_server_port) {
    std::cout << "Please set `--server_port`." << std::endl;
    return -1;
  }

  ServiceMulImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:" + std::to_string(FLAGS_server_port), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::cout << "The service is up on `localhost:" << FLAGS_server_port << "`. Ctrl+C to cancel." << std::endl;

  server->Wait();
}
