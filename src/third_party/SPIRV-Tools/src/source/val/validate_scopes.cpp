// Copyright (c) 2018 Google LLC.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/val/validate_scopes.h"

#include "source/diagnostic.h"
#include "source/spirv_target_env.h"
#include "source/val/instruction.h"
#include "source/val/validation_state.h"

namespace spvtools {
namespace val {

spv_result_t ValidateExecutionScope(ValidationState_t& _,
                                    const Instruction* inst, uint32_t scope) {
  SpvOp opcode = inst->opcode();
  bool is_int32 = false, is_const_int32 = false;
  uint32_t value = 0;
  std::tie(is_int32, is_const_int32, value) = _.EvalInt32IfConst(scope);

  if (!is_int32) {
    return _.diag(SPV_ERROR_INVALID_DATA, inst)
           << spvOpcodeString(opcode)
           << ": expected Execution Scope to be a 32-bit int";
  }

  if (!is_const_int32) {
    return SPV_SUCCESS;
  }

  // Vulkan specific rules
  if (spvIsVulkanEnv(_.context()->target_env)) {
    // Vulkan 1.1 specific rules
    if (_.context()->target_env != SPV_ENV_VULKAN_1_0) {
      // Scope for Non Uniform Group Operations must be limited to Subgroup
      if (spvOpcodeIsNonUniformGroupOperation(opcode) &&
          value != SpvScopeSubgroup) {
        return _.diag(SPV_ERROR_INVALID_DATA, inst)
               << spvOpcodeString(opcode)
               << ": in Vulkan environment Execution scope is limited to "
                  "Subgroup";
      }
    }

    // If OpControlBarrier is used in fragment, vertex, tessellation evaluation,
    // or geometry stages, the execution Scope must be Subgroup.
    if (opcode == SpvOpControlBarrier && value != SpvScopeSubgroup) {
      _.function(inst->function()->id())
          ->RegisterExecutionModelLimitation([](SpvExecutionModel model,
                                                std::string* message) {
            if (model == SpvExecutionModelFragment ||
                model == SpvExecutionModelVertex ||
                model == SpvExecutionModelGeometry ||
                model == SpvExecutionModelTessellationEvaluation) {
              if (message) {
                *message =
                    "in Vulkan evironment, OpControlBarrier execution scope "
                    "must be Subgroup for Fragment, Vertex, Geometry and "
                    "TessellationEvaluation execution models";
              }
              return false;
            }
            return true;
          });
    }

    // Vulkan generic rules
    // Scope for execution must be limited to Workgroup or Subgroup
    if (value != SpvScopeWorkgroup && value != SpvScopeSubgroup) {
      return _.diag(SPV_ERROR_INVALID_DATA, inst)
             << spvOpcodeString(opcode)
             << ": in Vulkan environment Execution Scope is limited to "
                "Workgroup and Subgroup";
    }
  }

  // TODO(atgoo@github.com) Add checks for OpenCL and OpenGL environments.

  // General SPIRV rules
  // Scope for execution must be limited to Workgroup or Subgroup for
  // non-uniform operations
  if (spvOpcodeIsNonUniformGroupOperation(opcode) &&
      value != SpvScopeSubgroup && value != SpvScopeWorkgroup) {
    return _.diag(SPV_ERROR_INVALID_DATA, inst)
           << spvOpcodeString(opcode)
           << ": Execution scope is limited to Subgroup or Workgroup";
  }

  return SPV_SUCCESS;
}

}  // namespace val
}  // namespace spvtools
