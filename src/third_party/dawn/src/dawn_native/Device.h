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

#ifndef DAWNNATIVE_DEVICEBASE_H_
#define DAWNNATIVE_DEVICEBASE_H_

#include "dawn_native/Error.h"
#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"

#include "dawn_native/DawnNative.h"
#include "dawn_native/dawn_platform.h"

#include <memory>

namespace dawn_native {

    using ErrorCallback = void (*)(const char* errorMessage, void* userData);

    class DeviceBase {
      public:
        DeviceBase();
        virtual ~DeviceBase();

        void HandleError(const char* message);

        bool ConsumedError(MaybeError maybeError) {
            if (DAWN_UNLIKELY(maybeError.IsError())) {
                ConsumeError(maybeError.AcquireError());
                return true;
            }
            return false;
        }

        // Used by autogenerated code, returns itself
        DeviceBase* GetDevice();

        virtual BindGroupBase* CreateBindGroup(BindGroupBuilder* builder) = 0;
        virtual BlendStateBase* CreateBlendState(BlendStateBuilder* builder) = 0;
        virtual BufferViewBase* CreateBufferView(BufferViewBuilder* builder) = 0;
        virtual CommandBufferBase* CreateCommandBuffer(CommandBufferBuilder* builder) = 0;
        virtual DepthStencilStateBase* CreateDepthStencilState(
            DepthStencilStateBuilder* builder) = 0;
        virtual InputStateBase* CreateInputState(InputStateBuilder* builder) = 0;
        virtual RenderPassDescriptorBase* CreateRenderPassDescriptor(
            RenderPassDescriptorBuilder* builder) = 0;
        virtual RenderPipelineBase* CreateRenderPipeline(RenderPipelineBuilder* builder) = 0;
        virtual SwapChainBase* CreateSwapChain(SwapChainBuilder* builder) = 0;

        virtual void TickImpl() = 0;

        // Many Dawn objects are completely immutable once created which means that if two
        // builders are given the same arguments, they can return the same object. Reusing
        // objects will help make comparisons between objects by a single pointer comparison.
        //
        // Technically no object is immutable as they have a reference count, and an
        // application with reference-counting issues could "see" that objects are reused.
        // This is solved by automatic-reference counting, and also the fact that when using
        // the client-server wire every creation will get a different proxy object, with a
        // different reference count.
        //
        // When trying to create an object, we give both the builder and an example of what
        // the built object will be, the "blueprint". The blueprint is just a FooBase object
        // instead of a backend Foo object. If the blueprint doesn't match an object in the
        // cache, then the builder is used to make a new object.
        ResultOrError<BindGroupLayoutBase*> GetOrCreateBindGroupLayout(
            const BindGroupLayoutDescriptor* descriptor);
        void UncacheBindGroupLayout(BindGroupLayoutBase* obj);

        // Dawn API
        BindGroupBuilder* CreateBindGroupBuilder();
        BindGroupLayoutBase* CreateBindGroupLayout(const BindGroupLayoutDescriptor* descriptor);
        BlendStateBuilder* CreateBlendStateBuilder();
        BufferBase* CreateBuffer(const BufferDescriptor* descriptor);
        CommandBufferBuilder* CreateCommandBufferBuilder();
        ComputePipelineBase* CreateComputePipeline(const ComputePipelineDescriptor* descriptor);
        DepthStencilStateBuilder* CreateDepthStencilStateBuilder();
        InputStateBuilder* CreateInputStateBuilder();
        PipelineLayoutBase* CreatePipelineLayout(const PipelineLayoutDescriptor* descriptor);
        QueueBase* CreateQueue();
        RenderPassDescriptorBuilder* CreateRenderPassDescriptorBuilder();
        RenderPipelineBuilder* CreateRenderPipelineBuilder();
        SamplerBase* CreateSampler(const SamplerDescriptor* descriptor);
        ShaderModuleBase* CreateShaderModule(const ShaderModuleDescriptor* descriptor);
        SwapChainBuilder* CreateSwapChainBuilder();
        TextureBase* CreateTexture(const TextureDescriptor* descriptor);
        TextureViewBase* CreateTextureView(TextureBase* texture,
                                           const TextureViewDescriptor* descriptor);

        void Tick();
        void SetErrorCallback(dawn::DeviceErrorCallback callback, dawn::CallbackUserdata userdata);
        void Reference();
        void Release();

        BufferBuilder* CreateBufferBuilderForTesting() {
            return nullptr;
        }

        virtual const PCIInfo& GetPCIInfo() const = 0;

      private:
        virtual ResultOrError<BindGroupLayoutBase*> CreateBindGroupLayoutImpl(
            const BindGroupLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<BufferBase*> CreateBufferImpl(const BufferDescriptor* descriptor) = 0;
        virtual ResultOrError<ComputePipelineBase*> CreateComputePipelineImpl(
            const ComputePipelineDescriptor* descriptor) = 0;
        virtual ResultOrError<PipelineLayoutBase*> CreatePipelineLayoutImpl(
            const PipelineLayoutDescriptor* descriptor) = 0;
        virtual ResultOrError<QueueBase*> CreateQueueImpl() = 0;
        virtual ResultOrError<SamplerBase*> CreateSamplerImpl(
            const SamplerDescriptor* descriptor) = 0;
        virtual ResultOrError<ShaderModuleBase*> CreateShaderModuleImpl(
            const ShaderModuleDescriptor* descriptor) = 0;
        virtual ResultOrError<TextureBase*> CreateTextureImpl(
            const TextureDescriptor* descriptor) = 0;
        virtual ResultOrError<TextureViewBase*> CreateTextureViewImpl(
            TextureBase* texture,
            const TextureViewDescriptor* descriptor) = 0;

        MaybeError CreateBindGroupLayoutInternal(BindGroupLayoutBase** result,
                                                 const BindGroupLayoutDescriptor* descriptor);
        MaybeError CreateBufferInternal(BufferBase** result, const BufferDescriptor* descriptor);
        MaybeError CreateComputePipelineInternal(ComputePipelineBase** result,
                                                 const ComputePipelineDescriptor* descriptor);
        MaybeError CreatePipelineLayoutInternal(PipelineLayoutBase** result,
                                                const PipelineLayoutDescriptor* descriptor);
        MaybeError CreateQueueInternal(QueueBase** result);
        MaybeError CreateSamplerInternal(SamplerBase** result, const SamplerDescriptor* descriptor);
        MaybeError CreateShaderModuleInternal(ShaderModuleBase** result,
                                              const ShaderModuleDescriptor* descriptor);
        MaybeError CreateTextureInternal(TextureBase** result, const TextureDescriptor* descriptor);
        MaybeError CreateTextureViewInternal(TextureViewBase** result,
                                             TextureBase* texture,
                                             const TextureViewDescriptor* descriptor);

        void ConsumeError(ErrorData* error);

        // The object caches aren't exposed in the header as they would require a lot of
        // additional includes.
        struct Caches;
        std::unique_ptr<Caches> mCaches;

        dawn::DeviceErrorCallback mErrorCallback = nullptr;
        dawn::CallbackUserdata mErrorUserdata = 0;
        uint32_t mRefCount = 1;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_DEVICEBASE_H_
