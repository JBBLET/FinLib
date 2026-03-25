// Copyright 2026 JBBLET
#pragma once

#define LOG_INFO(ctx, msg) (ctx).logger_->write(logging::Level::Info, msg)
#define LOG_WARN(ctx, msg) (ctx).logger_->write(logging::Level::Warning, msg)
#define LOG_ERROR(ctx, msg) (ctx).logger_->write(logging::Level::Error, msg)
#define LOG_DEBUG(ctx, msg) (ctx).logger_->write(logging::Level::Debug, msg)
