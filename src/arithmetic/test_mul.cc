#include "3rdparty/gtest/gtest-main-with-dflags.h"
#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "mul.grpc.pb.h"

DEFINE_uint16(port, 5001, "The port to use.");

TEST(Mul, Smoke) {
  struct MulServiceImpl final : test_mul::RPC::Service {
    grpc::Status Mul(grpc::ServerContext* context, test_mul::Req const* req, test_mul::Res* res) override {
      res->set_z(req->x() * req->y());
      return grpc::Status::OK;
    }
  };

  MulServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:" + std::to_string(FLAGS_port), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("localhost:" + std::to_string(FLAGS_port), grpc::InsecureChannelCredentials());

  std::unique_ptr<test_mul::RPC::Stub> stub(test_mul::RPC::NewStub(channel));

  test_mul::Req req;
  req.set_x(2);
  req.set_y(3);

  test_mul::Res res;

  grpc::Status const status = [&stub, &req, &res]() {
    grpc::ClientContext context;
    return stub->Mul(&context, req, &res);
  }();

  server = nullptr;

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(6, res.z());
}
