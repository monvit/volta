#include "volta_collector_client.h"
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <iostream>
#include <thread>
#include <chrono>

namespace volta {
namespace agent {
namespace client {

std::shared_ptr<grpc::Channel> VoltaCollectorClient::CreateChannel(const std::string &host) {
    return grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
}

void VoltaCollectorClient::SendMessage(const std::string &message) {
    grpc::ClientContext context;
    ::volta::Message request;
    request.set_message(message);

    ::volta::Response response;

    grpc::Status status = stub_->SendMessage(&context, request, &response);
    std::cout << response.response() << std::endl;
}

void VoltaCollectorClient::SendMessages() {
    grpc::ClientContext context;
    ::volta::Response response;

    std::unique_ptr<grpc::ClientWriter<::volta::Message>> writer(stub_->SendMessages(&context, &response));

    for (int i = 0; i < 10; i++) {
        ::volta::Message msg;
        msg.set_message("message number " + std::to_string(i));

        if(!writer->Write(msg)) {
            std::cout << "Something went wrong" << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    writer->WritesDone();
    grpc::Status status = writer->Finish();

    if(!status.ok()) {
        std::cout << "client stream rpc FAILED" << std::endl;
        std::cout << status.error_message() << std::endl;
        return;
    }

    std::cout << response.response() << std::endl;
}

void VoltaCollectorClient::GetResponses() {
    grpc::ClientContext context;
    ::volta::Message message;
    message.set_message("Gimme responses :)");

    std::unique_ptr<grpc::ClientReader<::volta::Response>> reader(stub_->GetResponses(&context, message));

    ::volta::Response response;
    while(reader->Read(&response)) {
        std::cout << response.response() << std::endl;
    }

    grpc::Status status = reader->Finish();

    if (!status.ok()) {
        std::cout << "server stream rpc FAILED" << std::endl;
        std::cout << status.error_message() << std::endl;
        return;
    }
}

void VoltaCollectorClient::Talk() {
    grpc::ClientContext context;

    std::shared_ptr<grpc::ClientReaderWriter<::volta::Message, ::volta::Response>> rw(stub_->Talk(&context));

    std::thread writeThread([rw]() {
        std::vector<std::string> messages {
            "message 1",
            "message 2",
            "message 3",
            "message 4",
            "message 5",
            "message 6",
            "message 7",
            "message 8",
            "message 9",
            "message 10"
        };

        for (const std::string& msg : messages) {
            ::volta::Message m;
            m.set_message(msg);
            rw->Write(m);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        rw->WritesDone();
    });

    ::volta::Response response;

    while(rw->Read(&response)) {
        std::cout << response.response() << std::endl;
    }

    writeThread.join();

    grpc::Status status = rw->Finish();

    if (!status.ok()) {
        std::cout << "client server stream rpc FAILED" << std::endl;
        std::cout << status.error_message() << " " << status.error_code() << std::endl;
        return;
    }
}

} // namespace client
} // namespace agent
} // namespace volta
