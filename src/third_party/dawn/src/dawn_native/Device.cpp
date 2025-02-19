// Copyright 2017 The Dawn Authors
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

#include "dawn_native/Device.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/BlendState.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/ComputePipeline.h"
#include "dawn_native/DepthStencilState.h"
#include "dawn_native/ErrorData.h"
#include "dawn_native/InputState.h"
#include "dawn_native/PipelineLayout.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassDescriptor.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Sampler.h"
#include "dawn_native/ShaderModule.h"
#include "dawn_native/SwapChain.h"
#include "dawn_native/Texture.h"

#include <unordered_set>

namespace dawn_native {

    // DeviceBase::Caches

    // The caches are unordered_sets of pointers with special hash and compare functions
    // to compare the value of the objects, instead of the pointers.
    using BindGroupLayoutCache = std::
        unordered_set<BindGroupLayoutBase*, BindGroupLayoutCacheFuncs, BindGroupLayoutCacheFuncs>;

    struct DeviceBase::Caches {
        BindGroupLayoutCache bindGroupLayouts;
    };

    // DeviceBase

    DeviceBase::DeviceBase() {
        mCaches = std::make_unique<DeviceBase::Caches>();
    }

    DeviceBase::~DeviceBase() {
    }

    void DeviceBase::HandleError(const char* message) {
        if (mErrorCallback) {
            mErrorCallback(message, mErrorUserdata);
        }
    }

    void DeviceBase::SetErrorCallback(dawn::DeviceErrorCallback callback,
                                      dawn::CallbackUserdata userdata) {
        mErrorCallback = callback;
        mErrorUserdata = userdata;
    }

    DeviceBase* DeviceBase::GetDevice() {
        return this;
    }

    ResultOrError<BindGroupLayoutBase*> DeviceBase::GetOrCreateBindGroupLayout(
        const BindGroupLayoutDescriptor* descriptor) {
        BindGroupLayoutBase blueprint(this, descriptor, true);

        auto iter = mCaches->bindGroupLayouts.find(&blueprint);
        if (iter != mCaches->bindGroupLayouts.end()) {
            (*iter)->Reference();
            return *iter;
        }

        BindGroupLayoutBase* backendObj;
        DAWN_TRY_ASSIGN(backendObj, CreateBindGroupLayoutImpl(descriptor));
        mCaches->bindGroupLayouts.insert(backendObj);
        return backendObj;
    }

    void DeviceBase::UncacheBindGroupLayout(BindGroupLayoutBase* obj) {
        mCaches->bindGroupLayouts.erase(obj);
    }

    // Object creation API methods

    BindGroupBuilder* DeviceBase::CreateBindGroupBuilder() {
        return new BindGroupBuilder(this);
    }
    BindGroupLayoutBase* DeviceBase::CreateBindGroupLayout(
        const BindGroupLayoutDescriptor* descriptor) {
        BindGroupLayoutBase* result = nullptr;

        if (ConsumedError(CreateBindGroupLayoutInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    BlendStateBuilder* DeviceBase::CreateBlendStateBuilder() {
        return new BlendStateBuilder(this);
    }
    BufferBase* DeviceBase::CreateBuffer(const BufferDescriptor* descriptor) {
        BufferBase* result = nullptr;

        if (ConsumedError(CreateBufferInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    CommandBufferBuilder* DeviceBase::CreateCommandBufferBuilder() {
        return new CommandBufferBuilder(this);
    }
    ComputePipelineBase* DeviceBase::CreateComputePipeline(
        const ComputePipelineDescriptor* descriptor) {
        ComputePipelineBase* result = nullptr;

        if (ConsumedError(CreateComputePipelineInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    DepthStencilStateBuilder* DeviceBase::CreateDepthStencilStateBuilder() {
        return new DepthStencilStateBuilder(this);
    }
    InputStateBuilder* DeviceBase::CreateInputStateBuilder() {
        return new InputStateBuilder(this);
    }
    PipelineLayoutBase* DeviceBase::CreatePipelineLayout(
        const PipelineLayoutDescriptor* descriptor) {
        PipelineLayoutBase* result = nullptr;

        if (ConsumedError(CreatePipelineLayoutInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    QueueBase* DeviceBase::CreateQueue() {
        QueueBase* result = nullptr;

        if (ConsumedError(CreateQueueInternal(&result))) {
            return nullptr;
        }

        return result;
    }
    RenderPassDescriptorBuilder* DeviceBase::CreateRenderPassDescriptorBuilder() {
        return new RenderPassDescriptorBuilder(this);
    }
    RenderPipelineBuilder* DeviceBase::CreateRenderPipelineBuilder() {
        return new RenderPipelineBuilder(this);
    }
    SamplerBase* DeviceBase::CreateSampler(const SamplerDescriptor* descriptor) {
        SamplerBase* result = nullptr;

        if (ConsumedError(CreateSamplerInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    ShaderModuleBase* DeviceBase::CreateShaderModule(const ShaderModuleDescriptor* descriptor) {
        ShaderModuleBase* result = nullptr;

        if (ConsumedError(CreateShaderModuleInternal(&result, descriptor))) {
            return nullptr;
        }

        return result;
    }
    SwapChainBuilder* DeviceBase::CreateSwapChainBuilder() {
        return new SwapChainBuilder(this);
    }
    TextureBase* DeviceBase::CreateTexture(const TextureDescriptor* descriptor) {
        TextureBase* result = nullptr;

        if (ConsumedError(CreateTextureInternal(&result, descriptor))) {
            return nullptr;
        }
        return result;
    }
    TextureViewBase* DeviceBase::CreateTextureView(TextureBase* texture,
                                                   const TextureViewDescriptor* descriptor) {
        TextureViewBase* result = nullptr;

        if (ConsumedError(CreateTextureViewInternal(&result, texture, descriptor))) {
            return nullptr;
        }
        return result;
    }

    // Other Device API methods

    void DeviceBase::Tick() {
        TickImpl();
    }

    void DeviceBase::Reference() {
        ASSERT(mRefCount != 0);
        mRefCount++;
    }

    void DeviceBase::Release() {
        ASSERT(mRefCount != 0);
        mRefCount--;
        if (mRefCount == 0) {
            delete this;
        }
    }

    // Implementation details of object creation

    MaybeError DeviceBase::CreateBindGroupLayoutInternal(
        BindGroupLayoutBase** result,
        const BindGroupLayoutDescriptor* descriptor) {
        DAWN_TRY(ValidateBindGroupLayoutDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, GetOrCreateBindGroupLayout(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateBufferInternal(BufferBase** result,
                                                const BufferDescriptor* descriptor) {
        DAWN_TRY(ValidateBufferDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateBufferImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateComputePipelineInternal(
        ComputePipelineBase** result,
        const ComputePipelineDescriptor* descriptor) {
        DAWN_TRY(ValidateComputePipelineDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateComputePipelineImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreatePipelineLayoutInternal(
        PipelineLayoutBase** result,
        const PipelineLayoutDescriptor* descriptor) {
        DAWN_TRY(ValidatePipelineLayoutDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreatePipelineLayoutImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateQueueInternal(QueueBase** result) {
        DAWN_TRY_ASSIGN(*result, CreateQueueImpl());
        return {};
    }

    MaybeError DeviceBase::CreateSamplerInternal(SamplerBase** result,
                                                 const SamplerDescriptor* descriptor) {
        DAWN_TRY(ValidateSamplerDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateSamplerImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateShaderModuleInternal(ShaderModuleBase** result,
                                                      const ShaderModuleDescriptor* descriptor) {
        DAWN_TRY(ValidateShaderModuleDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateShaderModuleImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateTextureInternal(TextureBase** result,
                                                 const TextureDescriptor* descriptor) {
        DAWN_TRY(ValidateTextureDescriptor(this, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateTextureImpl(descriptor));
        return {};
    }

    MaybeError DeviceBase::CreateTextureViewInternal(TextureViewBase** result,
                                                     TextureBase* texture,
                                                     const TextureViewDescriptor* descriptor) {
        DAWN_TRY(ValidateTextureViewDescriptor(this, texture, descriptor));
        DAWN_TRY_ASSIGN(*result, CreateTextureViewImpl(texture, descriptor));
        return {};
    }

    // Other implementation details

    void DeviceBase::ConsumeError(ErrorData* error) {
        ASSERT(error != nullptr);
        HandleError(error->GetMessage().c_str());
        delete error;
    }

}  // namespace dawn_native
