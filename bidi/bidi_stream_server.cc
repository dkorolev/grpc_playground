#include "grpcpp/grpcpp.h"
#include "bricks/dflags/dflags.h"
#include "bidi_stream.grpc.pb.h"
#include "blocks/xterm/vt100.h"

DEFINE_uint16(port, 50051, "The port to use.");

using namespace current::vt100;

int main(int argc, char** argv) {
#ifndef NDEBUG
  std::cout << bold << yellow << "WARNING" << reset << ": running a " << bold << red << "DEBUG" << reset
            << " build. Suboptimal for performance testing." << std::endl;
#endif

  ParseDFlags(&argc, &argv);

  struct StreamServiceImpl final : test_bidi_stream::RPCBidiStream::Service {
    grpc::Status Go(grpc::ServerContext*,
                    grpc::ServerReaderWriter<test_bidi_stream::Res, test_bidi_stream::Req>* stream) override {
      std::cerr << "Created a stream." << std::endl;

      test_bidi_stream::Req req;
      test_bidi_stream::Res res;

      while (stream->Read(&req)) {
        std::string s = req.s();
        int32_t i = req.i();
        int32_t c = req.c();
        int32_t n = req.n();

        if (i < 0) {
          i = 0;
        }
        if (i > static_cast<int32_t>(s.length())) {
          i = static_cast<int32_t>(s.length());
        }

        s = s.substr(i, c);

        res.set_id(req.id());
        if (n <= 1) {
          res.set_r(s);
        } else {
          std::ostringstream os;
          for (int32_t t = 0; t < n; ++t) {
            os << s;
          }
          res.set_r(os.str());
        }
        stream->Write(res);
      }

      std::cerr << "Closing a stream." << std::endl;
      return grpc::Status::OK;
    }
  };

  StreamServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort("0.0.0.0:" + std::to_string(FLAGS_port), grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  server->Wait();
}
