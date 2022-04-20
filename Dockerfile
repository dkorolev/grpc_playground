FROM ubuntu:focal-20220404

RUN apt-get update -y
RUN apt-get install -y bash git build-essential vim
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends tzdata
RUN apt-get install -y cmake

RUN git clone https://github.com/dkorolev/grpc_playground.git
RUN (cd grpc_playground; make release)

CMD "/grpc_playground/Release/test_add"
