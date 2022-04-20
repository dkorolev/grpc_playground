#include "3rdparty/gtest/gtest-main-with-dflags.h"
#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "add.grpc.pb.h"

DEFINE_uint16(port, 5001, "The port to use.");

TEST(Add, Smoke) {
  struct AddServiceImpl final : test_add::RPC::Service {
    grpc::Status Add(grpc::ServerContext* context, test_add::Req const* req, test_add::Res* res) override {
      res->set_c(req->a() + req->b());
      return grpc::Status::OK;
    }
  };

  AddServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:" + std::to_string(FLAGS_port), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel("localhost:" + std::to_string(FLAGS_port), grpc::InsecureChannelCredentials());

  std::unique_ptr<test_add::RPC::Stub> stub(test_add::RPC::NewStub(channel));

  test_add::Req req;
  req.set_a(2);
  req.set_b(3);

  test_add::Res res;

  grpc::Status const status = [&stub, &req, &res]() {
    grpc::ClientContext context;
    return stub->Add(&context, req, &res);
  }();

  server = nullptr;

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(5, res.c());
}
