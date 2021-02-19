#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"
#include <xbyak/xbyak.h>

#include <SimpleIni.h>
#include <frozen/set.h>
#include <nonstd/span.hpp>
#include <spdlog/sinks/basic_file_sink.h>

namespace logger = SKSE::log;
using namespace SKSE::util;
namespace stl = SKSE::stl;
using namespace std::string_view_literals;

#define DLLEXPORT __declspec(dllexport)

static constexpr std::string_view PapyrusUtil = "PapyrusUtil"sv;