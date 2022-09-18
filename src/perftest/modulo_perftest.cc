#include "grpc_perftest_main.h"

#include "modulo_schema.grpc.pb.h"

DEFINE_string(grpc_server, "localhost:5001", "The gRPC server to connect to.");

DEFINE_uint64(n, 1'000'000, "The number of test datapoints to generate for an interval.");

struct TestRunner final {
  using GRPCRequest = service_compute_modulo::Req;
  using GRPCResponse = service_compute_modulo::Res;
  using GRPCService = service_compute_modulo::RPC;

  using GoldenResponse = Optional<uint64_t>;

  template <typename F>
  static void GenerateData(F&& f) {
    for (uint64_t i = 0u; i < FLAGS_n; ++i) {
      uint64_t a = rand() % 1'000'000;
      uint64_t b = rand() % 1'000 + 2;
      GRPCRequest req;
      req.set_a(a);
      req.set_b(b);
      GoldenResponse res;
      if (b) {
        res = a % b;
      }
      f(req, res);
    }
  }

  inline static bool Validate(GRPCResponse const& response, GoldenResponse const& golden) {
    return Exists(golden) && response.c() == Value(golden);
  }

  inline static grpc::Status Run(typename GRPCService::Stub& stub,
                                 grpc::ClientContext* ctx,
                                 GRPCRequest const& req,
                                 GRPCResponse* res) {
    return stub.ComputeMod(ctx, req, res);
  }
};
