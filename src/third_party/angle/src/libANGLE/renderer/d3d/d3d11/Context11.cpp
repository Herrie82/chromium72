//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context11:
//   D3D11-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/d3d/d3d11/Context11.h"

#include "common/string_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Fence11.h"
#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/IndexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Program11.h"
#include "libANGLE/renderer/d3d/d3d11/ProgramPipeline11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"
#include "libANGLE/renderer/d3d/d3d11/TransformFeedback11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

namespace
{
ANGLE_INLINE bool DrawCallHasDynamicAttribs(const gl::Context *context)
{
    VertexArray11 *vertexArray11 = GetImplAs<VertexArray11>(context->getGLState().getVertexArray());
    return vertexArray11->hasActiveDynamicAttrib(context);
}

bool DrawCallHasStreamingVertexArrays(const gl::Context *context, gl::PrimitiveMode mode)
{
    // Direct drawing doesn't support dynamic attribute storage since it needs the first and count
    // to translate when applyVertexBuffer. GL_LINE_LOOP and GL_TRIANGLE_FAN are not supported
    // either since we need to simulate them in D3D.
    if (DrawCallHasDynamicAttribs(context) || mode == gl::PrimitiveMode::LineLoop ||
        mode == gl::PrimitiveMode::TriangleFan)
    {
        return true;
    }

    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(context->getGLState().getProgram());
    if (InstancedPointSpritesActive(programD3D, mode))
    {
        return true;
    }

    return false;
}

bool DrawCallHasStreamingElementArray(const gl::Context *context, GLenum srcType)
{
    const gl::State &glState       = context->getGLState();
    gl::Buffer *elementArrayBuffer = glState.getVertexArray()->getElementArrayBuffer();

    bool primitiveRestartWorkaround =
        UsePrimitiveRestartWorkaround(glState.isPrimitiveRestartEnabled(), srcType);
    const GLenum dstType = (srcType == GL_UNSIGNED_INT || primitiveRestartWorkaround)
                               ? GL_UNSIGNED_INT
                               : GL_UNSIGNED_SHORT;

    // Not clear where the offset comes from here.
    switch (ClassifyIndexStorage(glState, elementArrayBuffer, srcType, dstType, 0))
    {
        case IndexStorageType::Dynamic:
            return true;
        case IndexStorageType::Direct:
            return false;
        case IndexStorageType::Static:
        {
            BufferD3D *bufferD3D                     = GetImplAs<BufferD3D>(elementArrayBuffer);
            StaticIndexBufferInterface *staticBuffer = bufferD3D->getStaticIndexBuffer();
            return (staticBuffer->getBufferSize() == 0 || staticBuffer->getIndexType() != dstType);
        }
        default:
            UNREACHABLE();
            return true;
    }
}

template <typename IndirectBufferT>
angle::Result ReadbackIndirectBuffer(const gl::Context *context,
                                     const void *indirect,
                                     const IndirectBufferT **bufferPtrOut)
{
    const gl::State &glState       = context->getGLState();
    gl::Buffer *drawIndirectBuffer = glState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(drawIndirectBuffer);
    Buffer11 *storage = GetImplAs<Buffer11>(drawIndirectBuffer);
    uintptr_t offset  = reinterpret_cast<uintptr_t>(indirect);

    const uint8_t *bufferData = nullptr;
    ANGLE_TRY(storage->getData(context, &bufferData));
    ASSERT(bufferData);

    *bufferPtrOut = reinterpret_cast<const IndirectBufferT *>(bufferData + offset);
    return angle::Result::Continue();
}
}  // anonymous namespace

Context11::Context11(const gl::ContextState &state, Renderer11 *renderer)
    : ContextD3D(state), mRenderer(renderer)
{}

Context11::~Context11() {}

angle::Result Context11::initialize()
{
    return angle::Result::Continue();
}

void Context11::onDestroy(const gl::Context *context)
{
    mIncompleteTextures.onDestroy(context);
}

CompilerImpl *Context11::createCompiler()
{
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        return new CompilerD3D(SH_HLSL_4_0_FL9_3_OUTPUT);
    }
    else
    {
        return new CompilerD3D(SH_HLSL_4_1_OUTPUT);
    }
}

ShaderImpl *Context11::createShader(const gl::ShaderState &data)
{
    return new ShaderD3D(data, mRenderer->getWorkarounds(), mRenderer->getNativeExtensions());
}

ProgramImpl *Context11::createProgram(const gl::ProgramState &data)
{
    return new Program11(data, mRenderer);
}

FramebufferImpl *Context11::createFramebuffer(const gl::FramebufferState &data)
{
    return new Framebuffer11(data, mRenderer);
}

TextureImpl *Context11::createTexture(const gl::TextureState &state)
{
    switch (state.getType())
    {
        case gl::TextureType::_2D:
            return new TextureD3D_2D(state, mRenderer);
        case gl::TextureType::CubeMap:
            return new TextureD3D_Cube(state, mRenderer);
        case gl::TextureType::_3D:
            return new TextureD3D_3D(state, mRenderer);
        case gl::TextureType::_2DArray:
            return new TextureD3D_2DArray(state, mRenderer);
        case gl::TextureType::External:
            return new TextureD3D_External(state, mRenderer);
        case gl::TextureType::_2DMultisample:
            return new TextureD3D_2DMultisample(state, mRenderer);
        case gl::TextureType::_2DMultisampleArray:
            return new TextureD3D_2DMultisampleArray(state, mRenderer);
        default:
            UNREACHABLE();
    }

    return nullptr;
}

RenderbufferImpl *Context11::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferD3D(state, mRenderer);
}

BufferImpl *Context11::createBuffer(const gl::BufferState &state)
{
    Buffer11 *buffer = new Buffer11(state, mRenderer);
    mRenderer->onBufferCreate(buffer);
    return buffer;
}

VertexArrayImpl *Context11::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArray11(data);
}

QueryImpl *Context11::createQuery(gl::QueryType type)
{
    return new Query11(mRenderer, type);
}

FenceNVImpl *Context11::createFenceNV()
{
    return new FenceNV11(mRenderer);
}

SyncImpl *Context11::createSync()
{
    return new Sync11(mRenderer);
}

TransformFeedbackImpl *Context11::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedback11(state, mRenderer);
}

SamplerImpl *Context11::createSampler(const gl::SamplerState &state)
{
    return new SamplerD3D(state);
}

ProgramPipelineImpl *Context11::createProgramPipeline(const gl::ProgramPipelineState &data)
{
    return new ProgramPipeline11(data);
}

std::vector<PathImpl *> Context11::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

angle::Result Context11::flush(const gl::Context *context)
{
    return mRenderer->flush(this);
}

angle::Result Context11::finish(const gl::Context *context)
{
    return mRenderer->finish(this);
}

angle::Result Context11::drawArrays(const gl::Context *context,
                                    gl::PrimitiveMode mode,
                                    GLint first,
                                    GLsizei count)
{
    ASSERT(count > 0);
    ANGLE_TRY(mRenderer->getStateManager()->updateState(context, mode, first, count, GL_NONE,
                                                        nullptr, 0, 0));
    return mRenderer->drawArrays(context, mode, first, count, 0);
}

angle::Result Context11::drawArraysInstanced(const gl::Context *context,
                                             gl::PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instanceCount)
{
    ASSERT(count > 0);
    ANGLE_TRY(mRenderer->getStateManager()->updateState(context, mode, first, count, GL_NONE,
                                                        nullptr, instanceCount, 0));
    return mRenderer->drawArrays(context, mode, first, count, instanceCount);
}

ANGLE_INLINE angle::Result Context11::drawElementsImpl(const gl::Context *context,
                                                       gl::PrimitiveMode mode,
                                                       GLsizei indexCount,
                                                       GLenum indexType,
                                                       const void *indices,
                                                       GLsizei instanceCount)
{
    ASSERT(indexCount > 0);

    if (DrawCallHasDynamicAttribs(context))
    {
        gl::IndexRange indexRange;
        ANGLE_TRY(context->getGLState().getVertexArray()->getIndexRange(
            context, indexType, indexCount, indices, &indexRange));
        ANGLE_TRY(mRenderer->getStateManager()->updateState(
            context, mode, indexRange.start, indexCount, indexType, indices, instanceCount, 0));
        return mRenderer->drawElements(context, mode, indexRange.start, indexCount, indexType,
                                       indices, instanceCount);
    }
    else
    {
        ANGLE_TRY(mRenderer->getStateManager()->updateState(context, mode, 0, indexCount, indexType,
                                                            indices, instanceCount, 0));
        return mRenderer->drawElements(context, mode, 0, indexCount, indexType, indices,
                                       instanceCount);
    }
}

angle::Result Context11::drawElements(const gl::Context *context,
                                      gl::PrimitiveMode mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void *indices)
{
    return drawElementsImpl(context, mode, count, type, indices, 0);
}

angle::Result Context11::drawElementsInstanced(const gl::Context *context,
                                               gl::PrimitiveMode mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void *indices,
                                               GLsizei instances)
{
    return drawElementsImpl(context, mode, count, type, indices, instances);
}

angle::Result Context11::drawRangeElements(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           GLenum type,
                                           const void *indices)
{
    return drawElementsImpl(context, mode, count, type, indices, 0);
}

angle::Result Context11::drawArraysIndirect(const gl::Context *context,
                                            gl::PrimitiveMode mode,
                                            const void *indirect)
{
    if (DrawCallHasStreamingVertexArrays(context, mode))
    {
        const gl::DrawArraysIndirectCommand *cmd = nullptr;
        ANGLE_TRY(ReadbackIndirectBuffer(context, indirect, &cmd));

        ANGLE_TRY(mRenderer->getStateManager()->updateState(
            context, mode, cmd->first, cmd->count, GL_NONE, nullptr, cmd->instanceCount, 0));
        return mRenderer->drawArrays(context, mode, cmd->first, cmd->count, cmd->instanceCount);
    }
    else
    {
        ANGLE_TRY(
            mRenderer->getStateManager()->updateState(context, mode, 0, 0, GL_NONE, nullptr, 0, 0));
        return mRenderer->drawArraysIndirect(context, indirect);
    }
}

angle::Result Context11::drawElementsIndirect(const gl::Context *context,
                                              gl::PrimitiveMode mode,
                                              GLenum type,
                                              const void *indirect)
{
    if (DrawCallHasStreamingVertexArrays(context, mode) ||
        DrawCallHasStreamingElementArray(context, type))
    {
        const gl::DrawElementsIndirectCommand *cmd = nullptr;
        ANGLE_TRY(ReadbackIndirectBuffer(context, indirect, &cmd));

        const gl::Type &typeInfo = gl::GetTypeInfo(type);
        const void *indices      = reinterpret_cast<const void *>(
            static_cast<uintptr_t>(cmd->firstIndex * typeInfo.bytes));

        // We must explicitly resolve the index range for the slow-path indirect drawElements to
        // make sure we are using the correct 'baseVertex'. This parameter does not exist for the
        // direct drawElements.
        gl::IndexRange indexRange;
        ANGLE_TRY(context->getGLState().getVertexArray()->getIndexRange(context, type, cmd->count,
                                                                        indices, &indexRange));

        GLint startVertex;
        ANGLE_TRY(ComputeStartVertex(GetImplAs<Context11>(context), indexRange, cmd->baseVertex,
                                     &startVertex));

        ANGLE_TRY(mRenderer->getStateManager()->updateState(context, mode, startVertex, cmd->count,
                                                            type, indices, cmd->primCount,
                                                            cmd->baseVertex));
        return mRenderer->drawElements(context, mode, indexRange.start, cmd->count, type, indices,
                                       cmd->primCount);
    }
    else
    {
        ANGLE_TRY(
            mRenderer->getStateManager()->updateState(context, mode, 0, 0, type, nullptr, 0, 0));
        return mRenderer->drawElementsIndirect(context, indirect);
    }
}

GLenum Context11::getResetStatus()
{
    return mRenderer->getResetStatus();
}

std::string Context11::getVendorString() const
{
    return mRenderer->getVendorString();
}

std::string Context11::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void Context11::insertEventMarker(GLsizei length, const char *marker)
{
    mRenderer->getAnnotator()->setMarker(marker);
}

void Context11::pushGroupMarker(GLsizei length, const char *marker)
{
    mRenderer->getAnnotator()->beginEvent(marker, marker);
    mMarkerStack.push(std::string(marker));
}

void Context11::popGroupMarker()
{
    const char *marker = nullptr;
    if (!mMarkerStack.empty())
    {
        marker = mMarkerStack.top().c_str();
        mMarkerStack.pop();
        mRenderer->getAnnotator()->endEvent(marker);
    }
}

void Context11::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    // Fall through to the EXT_debug_marker functions
    pushGroupMarker(length, message);
}

void Context11::popDebugGroup()
{
    // Fall through to the EXT_debug_marker functions
    popGroupMarker();
}

angle::Result Context11::syncState(const gl::Context *context,
                                   const gl::State::DirtyBits &dirtyBits,
                                   const gl::State::DirtyBits &bitMask)
{
    mRenderer->getStateManager()->syncState(context, dirtyBits);
    return angle::Result::Continue();
}

GLint Context11::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 Context11::getTimestamp()
{
    return mRenderer->getTimestamp();
}

angle::Result Context11::onMakeCurrent(const gl::Context *context)
{
    return mRenderer->getStateManager()->onMakeCurrent(context);
}

gl::Caps Context11::getNativeCaps() const
{
    gl::Caps caps = mRenderer->getNativeCaps();

    // For pixel shaders, the render targets and unordered access views share the same resource
    // slots, so the maximum number of fragment shader outputs depends on the current context
    // version:
    // - If current context is ES 3.0 and below, we use D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT(8)
    //   as the value of max draw buffers because UAVs are not used.
    // - If current context is ES 3.1 and the feature level is 11_0, the RTVs and UAVs share 8
    //   slots. As ES 3.1 requires at least 1 atomic counter buffer in compute shaders, the value
    //   of max combined shader output resources is limited to 7, thus only 7 RTV slots can be
    //   used simultaneously.
    // - If current context is ES 3.1 and the feature level is 11_1, the RTVs and UAVs share 64
    //   slots. Currently we allocate 60 slots for combined shader output resources, so we can use
    //   at most D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT(8) RTVs simultaneously.
    if (mState.getClientVersion() >= gl::ES_3_1 &&
        mRenderer->getRenderer11DeviceCaps().featureLevel == D3D_FEATURE_LEVEL_11_0)
    {
        caps.maxDrawBuffers      = caps.maxCombinedShaderOutputResources;
        caps.maxColorAttachments = caps.maxCombinedShaderOutputResources;
    }

    return caps;
}

const gl::TextureCapsMap &Context11::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &Context11::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &Context11::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

angle::Result Context11::dispatchCompute(const gl::Context *context,
                                         GLuint numGroupsX,
                                         GLuint numGroupsY,
                                         GLuint numGroupsZ)
{
    return mRenderer->dispatchCompute(context, numGroupsX, numGroupsY, numGroupsZ);
}

angle::Result Context11::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    return mRenderer->dispatchComputeIndirect(context, indirect);
}

angle::Result Context11::triggerDrawCallProgramRecompilation(const gl::Context *context,
                                                             gl::PrimitiveMode drawMode)
{
    const auto &glState    = context->getGLState();
    const auto *va11       = GetImplAs<VertexArray11>(glState.getVertexArray());
    const auto *drawFBO    = glState.getDrawFramebuffer();
    gl::Program *program   = glState.getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    programD3D->updateCachedInputLayout(va11->getCurrentStateSerial(), glState);
    programD3D->updateCachedOutputLayout(context, drawFBO);

    bool recompileVS = !programD3D->hasVertexExecutableForCachedInputLayout();
    bool recompileGS = !programD3D->hasGeometryExecutableForPrimitiveType(drawMode);
    bool recompilePS = !programD3D->hasPixelExecutableForCachedOutputLayout();

    if (!recompileVS && !recompileGS && !recompilePS)
    {
        return angle::Result::Continue();
    }

    // Load the compiler if necessary and recompile the programs.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized(this));

    gl::InfoLog infoLog;

    if (recompileVS)
    {
        ShaderExecutableD3D *vertexExe = nullptr;
        ANGLE_TRY(programD3D->getVertexExecutableForCachedInputLayout(this, &vertexExe, &infoLog));
        if (!programD3D->hasVertexExecutableForCachedInputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic vertex executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic vertex executable");
        }
    }

    if (recompileGS)
    {
        ShaderExecutableD3D *geometryExe = nullptr;
        ANGLE_TRY(programD3D->getGeometryExecutableForPrimitiveType(
            this, mState.getCaps(), drawMode, &geometryExe, &infoLog));
        if (!programD3D->hasGeometryExecutableForPrimitiveType(drawMode))
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic geometry executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic geometry executable");
        }
    }

    if (recompilePS)
    {
        ShaderExecutableD3D *pixelExe = nullptr;
        ANGLE_TRY(programD3D->getPixelExecutableForCachedOutputLayout(this, &pixelExe, &infoLog));
        if (!programD3D->hasPixelExecutableForCachedOutputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic pixel executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic pixel executable");
        }
    }

    // Refresh the program cache entry.
    if (mMemoryProgramCache)
    {
        mMemoryProgramCache->updateProgram(context, program);
    }

    return angle::Result::Continue();
}

angle::Result Context11::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue();
}

angle::Result Context11::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    return angle::Result::Continue();
}

angle::Result Context11::getIncompleteTexture(const gl::Context *context,
                                              gl::TextureType type,
                                              gl::Texture **textureOut)
{
    return mIncompleteTextures.getIncompleteTexture(context, type, this, textureOut);
}

angle::Result Context11::initializeMultisampleTextureToBlack(const gl::Context *context,
                                                             gl::Texture *glTexture)
{
    ASSERT(glTexture->getType() == gl::TextureType::_2DMultisample);
    TextureD3D *textureD3D        = GetImplAs<TextureD3D>(glTexture);
    gl::ImageIndex index          = gl::ImageIndex::Make2DMultisample();
    RenderTargetD3D *renderTarget = nullptr;
    ANGLE_TRY(textureD3D->getRenderTarget(context, index, &renderTarget));
    return mRenderer->clearRenderTarget(context, renderTarget, gl::ColorF(0.0f, 0.0f, 0.0f, 1.0f),
                                        1.0f, 0);
}

void Context11::handleResult(HRESULT hr,
                             const char *message,
                             const char *file,
                             const char *function,
                             unsigned int line)
{
    ASSERT(FAILED(hr));

    if (d3d11::isDeviceLostError(hr))
    {
        mRenderer->notifyDeviceLost();
    }

    GLenum glErrorCode = DefaultGLErrorCode(hr);

    std::stringstream errorStream;
    errorStream << "Internal D3D11 error: " << gl::FmtHR(hr) << ": " << message;

    mErrors->handleError(glErrorCode, errorStream.str().c_str(), file, function, line);
}
}  // namespace rx
