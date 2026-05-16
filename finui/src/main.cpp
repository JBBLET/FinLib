#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>
#include "helloWorld.grpc.pb.h"

int main() {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    auto stub = helloworld::Greeter::NewStub(channel);

    helloworld::HelloRequest request;
    request.set_name("FinUI");

    helloworld::HelloReply reply;
    grpc::ClientContext context;

    grpc::Status status = stub->SayHello(&context, request, &reply);

    if (status.ok()) {
        std::cout << "Response: " << reply.message() << "\n";
    } else {
        std::cerr << "RPC failed: " << status.error_message() << "\n";
        return 1;
    }

    return 0;
}
