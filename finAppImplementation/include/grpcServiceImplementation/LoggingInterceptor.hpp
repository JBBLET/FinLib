// Copyright (c) 2026 JBBLET. All Rights Reserved.
#pragma once

#include <grpcpp/support/interceptor.h>
#include <grpcpp/support/server_interceptor.h>

#include <chrono>
#include <string>

#include "finapp/common/logger/ILogger.hpp"

class LoggingInterceptor : public grpc::experimental::Interceptor {
 public:
    LoggingInterceptor(grpc::experimental::ServerRpcInfo* info, finapp::logging::ILogger* logger)
        : method_(info->method()), logger_(logger) {}

    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        using Hook = grpc::experimental::InterceptionHookPoints;

        if (methods->QueryInterceptionHookPoint(Hook::POST_RECV_INITIAL_METADATA)) {
            start_ = std::chrono::steady_clock::now();
            logger_->write(finapp::logging::Level::Info, std::string("[gRPC >>>] ") + method_);
        }

        if (methods->QueryInterceptionHookPoint(Hook::PRE_SEND_STATUS)) {
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - start_)
                                       .count();
            grpc::Status status = methods->GetSendStatus();
            const bool ok = status.ok();
            const std::string statusStr = ok ? "OK" : status.error_message();
            logger_->write(ok ? finapp::logging::Level::Info : finapp::logging::Level::Error,
                           std::string("[gRPC <<<] ") + method_ + " [" + statusStr + "] " +
                               std::to_string(elapsedMs) + "ms");
        }

        methods->Proceed();
    }

 private:
    std::string method_;
    finapp::logging::ILogger* logger_;
    std::chrono::steady_clock::time_point start_{};
};

class LoggingInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
 public:
    explicit LoggingInterceptorFactory(finapp::logging::ILogger* logger) : logger_(logger) {}

    grpc::experimental::Interceptor* CreateServerInterceptor(
        grpc::experimental::ServerRpcInfo* info) override {
        return new LoggingInterceptor(info, logger_);
    }

 private:
    finapp::logging::ILogger* logger_;
};
