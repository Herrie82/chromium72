// Copyright 2018 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef VK_COMMAND_POOL_HPP_
#define VK_COMMAND_POOL_HPP_

#include "VkObject.hpp"

namespace vk
{

class CommandPool : public Object<CommandPool, VkCommandPool>
{
public:
	CommandPool(const VkCommandPoolCreateInfo* pCreateInfo, void* mem);
	~CommandPool() = delete;
	void destroy(const VkAllocationCallbacks* pAllocator);

	static size_t ComputeRequiredAllocationSize(const VkCommandPoolCreateInfo* pCreateInfo);

private:
};

static inline CommandPool* Cast(VkCommandPool object)
{
	return reinterpret_cast<CommandPool*>(object);
}

} // namespace vk

#endif // VK_COMMAND_POOL_HPP_
