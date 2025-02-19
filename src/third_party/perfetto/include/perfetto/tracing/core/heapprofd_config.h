/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*******************************************************************************
 * AUTOGENERATED - DO NOT EDIT
 *******************************************************************************
 * This file has been generated from the protobuf message
 * perfetto/config/profiling/heapprofd_config.proto
 * by
 * ../../tools/proto_to_cpp/proto_to_cpp.cc.
 * If you need to make changes here, change the .proto file and then run
 * ./tools/gen_tracing_cpp_headers_from_protos
 */

#ifndef INCLUDE_PERFETTO_TRACING_CORE_HEAPPROFD_CONFIG_H_
#define INCLUDE_PERFETTO_TRACING_CORE_HEAPPROFD_CONFIG_H_

#include <stdint.h>
#include <string>
#include <type_traits>
#include <vector>

#include "perfetto/base/export.h"

// Forward declarations for protobuf types.
namespace perfetto {
namespace protos {
class HeapprofdConfig;
class HeapprofdConfig_ContinousDumpConfig;
}  // namespace protos
}  // namespace perfetto

namespace perfetto {

class PERFETTO_EXPORT HeapprofdConfig {
 public:
  class PERFETTO_EXPORT ContinousDumpConfig {
   public:
    ContinousDumpConfig();
    ~ContinousDumpConfig();
    ContinousDumpConfig(ContinousDumpConfig&&) noexcept;
    ContinousDumpConfig& operator=(ContinousDumpConfig&&);
    ContinousDumpConfig(const ContinousDumpConfig&);
    ContinousDumpConfig& operator=(const ContinousDumpConfig&);

    // Conversion methods from/to the corresponding protobuf types.
    void FromProto(
        const perfetto::protos::HeapprofdConfig_ContinousDumpConfig&);
    void ToProto(perfetto::protos::HeapprofdConfig_ContinousDumpConfig*) const;

    uint32_t dump_phase_ms() const { return dump_phase_ms_; }
    void set_dump_phase_ms(uint32_t value) { dump_phase_ms_ = value; }

    uint32_t dump_interval_ms() const { return dump_interval_ms_; }
    void set_dump_interval_ms(uint32_t value) { dump_interval_ms_ = value; }

   private:
    uint32_t dump_phase_ms_ = {};
    uint32_t dump_interval_ms_ = {};

    // Allows to preserve unknown protobuf fields for compatibility
    // with future versions of .proto files.
    std::string unknown_fields_;
  };

  HeapprofdConfig();
  ~HeapprofdConfig();
  HeapprofdConfig(HeapprofdConfig&&) noexcept;
  HeapprofdConfig& operator=(HeapprofdConfig&&);
  HeapprofdConfig(const HeapprofdConfig&);
  HeapprofdConfig& operator=(const HeapprofdConfig&);

  // Conversion methods from/to the corresponding protobuf types.
  void FromProto(const perfetto::protos::HeapprofdConfig&);
  void ToProto(perfetto::protos::HeapprofdConfig*) const;

  uint64_t sampling_interval_bytes() const { return sampling_interval_bytes_; }
  void set_sampling_interval_bytes(uint64_t value) {
    sampling_interval_bytes_ = value;
  }

  int process_cmdline_size() const {
    return static_cast<int>(process_cmdline_.size());
  }
  const std::vector<std::string>& process_cmdline() const {
    return process_cmdline_;
  }
  std::string* add_process_cmdline() {
    process_cmdline_.emplace_back();
    return &process_cmdline_.back();
  }

  int pid_size() const { return static_cast<int>(pid_.size()); }
  const std::vector<uint64_t>& pid() const { return pid_; }
  uint64_t* add_pid() {
    pid_.emplace_back();
    return &pid_.back();
  }

  bool all() const { return all_; }
  void set_all(bool value) { all_ = value; }

  const ContinousDumpConfig& continuous_dump_config() const {
    return continuous_dump_config_;
  }
  ContinousDumpConfig* mutable_continuous_dump_config() {
    return &continuous_dump_config_;
  }

 private:
  uint64_t sampling_interval_bytes_ = {};
  std::vector<std::string> process_cmdline_;
  std::vector<uint64_t> pid_;
  bool all_ = {};
  ContinousDumpConfig continuous_dump_config_ = {};

  // Allows to preserve unknown protobuf fields for compatibility
  // with future versions of .proto files.
  std::string unknown_fields_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_CORE_HEAPPROFD_CONFIG_H_
