/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGLGpu_DEFINED
#define GrGLGpu_DEFINED

#include "GrGLContext.h"
#include "GrGLIRect.h"
#include "GrGLPathRendering.h"
#include "GrGLProgram.h"
#include "GrGLRenderTarget.h"
#include "GrGLStencilAttachment.h"
#include "GrGLTexture.h"
#include "GrGLVertexArray.h"
#include "GrGpu.h"
#include "GrMesh.h"
#include "GrWindowRectsState.h"
#include "GrXferProcessor.h"
#include "SkLRUCache.h"
#include "SkTArray.h"
#include "SkTypes.h"

class GrGLBuffer;
class GrGLGpuRTCommandBuffer;
class GrGLGpuTextureCommandBuffer;
class GrPipeline;
class GrSwizzle;

#ifdef SK_DEBUG
#define PROGRAM_CACHE_STATS
#endif

class GrGLGpu final : public GrGpu, private GrMesh::SendToGpuImpl {
public:
    static sk_sp<GrGpu> Make(sk_sp<const GrGLInterface>, const GrContextOptions&, GrContext*);
    ~GrGLGpu() override;

    void disconnect(DisconnectType) override;

    const GrGLContext& glContext() const { return *fGLContext; }

    const GrGLInterface* glInterface() const { return fGLContext->interface(); }
    const GrGLContextInfo& ctxInfo() const { return *fGLContext; }
    GrGLStandard glStandard() const { return fGLContext->standard(); }
    GrGLVersion glVersion() const { return fGLContext->version(); }
    GrGLSLGeneration glslGeneration() const { return fGLContext->glslGeneration(); }
    const GrGLCaps& glCaps() const { return *fGLContext->caps(); }

    GrGLPathRendering* glPathRendering() {
        SkASSERT(glCaps().shaderCaps()->pathRenderingSupport());
        return static_cast<GrGLPathRendering*>(pathRendering());
    }

    // Used by GrGLProgram to configure OpenGL state.
    void bindTexture(int unitIdx, GrSamplerState samplerState, GrGLTexture* texture);

    // These functions should be used to bind GL objects. They track the GL state and skip redundant
    // bindings. Making the equivalent glBind calls directly will confuse the state tracking.
    void bindVertexArray(GrGLuint id) {
        fHWVertexArrayState.setVertexArrayID(this, id);
    }

    // These callbacks update state tracking when GL objects are deleted. They are called from
    // GrGLResource onRelease functions.
    void notifyVertexArrayDelete(GrGLuint id) {
        fHWVertexArrayState.notifyVertexArrayDelete(id);
    }

    // Binds a buffer to the GL target corresponding to 'type', updates internal state tracking, and
    // returns the GL target the buffer was bound to.
    // When 'type' is kIndex_GrBufferType, this function will also implicitly bind the default VAO.
    // If the caller wishes to bind an index buffer to a specific VAO, it can call glBind directly.
    GrGLenum bindBuffer(GrBufferType type, const GrBuffer*);

    // The GrGLGpuRTCommandBuffer does not buffer up draws before submitting them to the gpu.
    // Thus this is the implementation of the draw call for the corresponding passthrough function
    // on GrGLRTGpuCommandBuffer.
    void draw(const GrPrimitiveProcessor&,
              const GrPipeline&,
              const GrPipeline::FixedDynamicState*,
              const GrPipeline::DynamicStateArrays*,
              const GrMesh[],
              int meshCount);

    // GrMesh::SendToGpuImpl methods. These issue the actual GL draw calls.
    // Marked final as a hint to the compiler to not use virtual dispatch.
    void sendMeshToGpu(GrPrimitiveType, const GrBuffer* vertexBuffer, int vertexCount,
                       int baseVertex) final;

    void sendIndexedMeshToGpu(GrPrimitiveType, const GrBuffer* indexBuffer, int indexCount,
                              int baseIndex, uint16_t minIndexValue, uint16_t maxIndexValue,
                              const GrBuffer* vertexBuffer, int baseVertex,
                              GrPrimitiveRestart) final;

    void sendInstancedMeshToGpu(GrPrimitiveType, const GrBuffer* vertexBuffer, int vertexCount,
                                int baseVertex, const GrBuffer* instanceBuffer, int instanceCount,
                                int baseInstance) final;

    void sendIndexedInstancedMeshToGpu(GrPrimitiveType, const GrBuffer* indexBuffer, int indexCount,
                                       int baseIndex, const GrBuffer* vertexBuffer, int baseVertex,
                                       const GrBuffer* instanceBuffer, int instanceCount,
                                       int baseInstance, GrPrimitiveRestart) final;

    // The GrGLGpuRTCommandBuffer does not buffer up draws before submitting them to the gpu.
    // Thus this is the implementation of the clear call for the corresponding passthrough function
    // on GrGLGpuRTCommandBuffer.
    void clear(const GrFixedClip&, const SkPMColor4f&, GrRenderTarget*, GrSurfaceOrigin);
    void clearColorAsDraw(const GrFixedClip&, const SkPMColor4f& color, GrRenderTarget*,
                          GrSurfaceOrigin);

    // The GrGLGpuRTCommandBuffer does not buffer up draws before submitting them to the gpu.
    // Thus this is the implementation of the clearStencil call for the corresponding passthrough
    // function on GrGLGpuRTCommandBuffer.
    void clearStencilClip(const GrFixedClip&, bool insideStencilMask,
                          GrRenderTarget*, GrSurfaceOrigin);

    void clearStencil(GrRenderTarget*, int clearValue);

    GrGpuRTCommandBuffer* getCommandBuffer(
            GrRenderTarget*, GrSurfaceOrigin, const SkRect&,
            const GrGpuRTCommandBuffer::LoadAndStoreInfo&,
            const GrGpuRTCommandBuffer::StencilLoadAndStoreInfo&) override;

    GrGpuTextureCommandBuffer* getCommandBuffer(GrTexture*, GrSurfaceOrigin) override;

    void invalidateBoundRenderTarget() {
        fHWBoundRenderTargetUniqueID.makeInvalid();
    }

    GrStencilAttachment* createStencilAttachmentForRenderTarget(const GrRenderTarget* rt,
                                                                int width,
                                                                int height) override;
#if GR_TEST_UTILS
    GrBackendTexture createTestingOnlyBackendTexture(const void* pixels, int w, int h,
                                                     GrColorType colorType, bool isRenderTarget,
                                                     GrMipMapped mipMapped,
                                                     size_t rowBytes = 0) override;
    bool isTestingOnlyBackendTexture(const GrBackendTexture&) const override;
    void deleteTestingOnlyBackendTexture(const GrBackendTexture&) override;

    GrBackendRenderTarget createTestingOnlyBackendRenderTarget(int w, int h, GrColorType) override;

    void deleteTestingOnlyBackendRenderTarget(const GrBackendRenderTarget&) override;

    const GrGLContext* glContextForTesting() const override { return &this->glContext(); }

    void resetShaderCacheForTesting() const override { fProgramCache->abandon(); }

    void testingOnly_flushGpuAndSync() override;
#endif

    void submit(GrGpuCommandBuffer* buffer) override;

    GrFence SK_WARN_UNUSED_RESULT insertFence() override;
    bool waitFence(GrFence, uint64_t timeout) override;
    void deleteFence(GrFence) const override;

    sk_sp<GrSemaphore> SK_WARN_UNUSED_RESULT makeSemaphore(bool isOwned) override;
    sk_sp<GrSemaphore> wrapBackendSemaphore(const GrBackendSemaphore& semaphore,
                                            GrResourceProvider::SemaphoreWrapType wrapType,
                                            GrWrapOwnership ownership) override;
    void insertSemaphore(sk_sp<GrSemaphore> semaphore, bool flush) override;
    void waitSemaphore(sk_sp<GrSemaphore> semaphore) override;

    sk_sp<GrSemaphore> prepareTextureForCrossContextUsage(GrTexture*) override;

    void deleteSync(GrGLsync) const;

    void insertEventMarker(const char*);

    void bindFramebuffer(GrGLenum fboTarget, GrGLuint fboid);
    void deleteFramebuffer(GrGLuint fboid);

private:
    GrGLGpu(std::unique_ptr<GrGLContext>, GrContext*);

    // GrGpu overrides
    void onResetContext(uint32_t resetBits) override;

    void xferBarrier(GrRenderTarget*, GrXferBarrierType) override;

    sk_sp<GrTexture> onCreateTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                     const GrMipLevel texels[], int mipLevelCount) override;

    GrBuffer* onCreateBuffer(size_t size, GrBufferType intendedType, GrAccessPattern,
                             const void* data) override;

    sk_sp<GrTexture> onWrapBackendTexture(const GrBackendTexture&, GrWrapOwnership,
                                          bool purgeImmediately) override;
    sk_sp<GrTexture> onWrapRenderableBackendTexture(const GrBackendTexture&,
                                                    int sampleCnt,
                                                    GrWrapOwnership) override;
    sk_sp<GrRenderTarget> onWrapBackendRenderTarget(const GrBackendRenderTarget&) override;
    sk_sp<GrRenderTarget> onWrapBackendTextureAsRenderTarget(const GrBackendTexture&,
                                                             int sampleCnt) override;

    // Given a GrPixelConfig return the index into the stencil format array on GrGLCaps to a
    // compatible stencil format, or negative if there is no compatible stencil format.
    int getCompatibleStencilIndex(GrPixelConfig config);

    void onFBOChanged();

    // Returns whether the texture is successfully created. On success, the
    // result is stored in |info|.
    // The texture is populated with |texels|, if it exists.
    // The texture parameters are cached in |initialTexParams|.
    bool createTextureImpl(const GrSurfaceDesc& desc, GrGLTextureInfo* info, bool renderTarget,
                           GrGLTexture::SamplerParams* initialTexParams, const GrMipLevel texels[],
                           int mipLevelCount, GrMipMapsStatus* mipMapsStatus);

    // Checks whether glReadPixels can be called to get pixel values in readConfig from the
    // render target.
    bool readPixelsSupported(GrRenderTarget* target, GrPixelConfig readConfig);

    // Checks whether glReadPixels can be called to get pixel values in readConfig from a
    // render target that has renderTargetConfig. This may have to create a temporary
    // render target and thus is less preferable than the variant that takes a render target.
    bool readPixelsSupported(GrPixelConfig renderTargetConfig, GrPixelConfig readConfig);

    // Checks whether glReadPixels can be called to get pixel values in readConfig from a
    // render target that has the same config as surfaceForConfig. Calls one of the the two
    // variations above, depending on whether the surface is a render target or not.
    bool readPixelsSupported(GrSurface* surfaceForConfig, GrPixelConfig readConfig);

    bool onReadPixels(GrSurface*, int left, int top, int width, int height, GrColorType,
                      void* buffer, size_t rowBytes) override;

    bool onWritePixels(GrSurface*, int left, int top, int width, int height, GrColorType,
                       const GrMipLevel texels[], int mipLevelCount) override;

    bool onTransferPixels(GrTexture*, int left, int top, int width, int height, GrColorType,
                          GrBuffer* transferBuffer, size_t offset, size_t rowBytes) override;

    // Before calling any variation of TexImage, TexSubImage, etc..., call this to ensure that the
    // PIXEL_UNPACK_BUFFER is unbound.
    void unbindCpuToGpuXferBuffer();

    void onResolveRenderTarget(GrRenderTarget* target) override;

    bool onRegenerateMipMapLevels(GrTexture*) override;

    bool onCopySurface(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                       GrSurface* src, GrSurfaceOrigin srcOrigin,
                       const SkIRect& srcRect, const SkIPoint& dstPoint,
                       bool canDiscardOutsideDstRect) override;

    // binds texture unit in GL
    void setTextureUnit(int unitIdx);

    void setTextureSwizzle(int unitIdx, GrGLenum target, const GrGLenum swizzle[]);

    /**
     * primitiveProcessorTextures must contain GrPrimitiveProcessor::numTextureSamplers() *
     * numPrimitiveProcessorTextureSets entries.
     */
    void resolveAndGenerateMipMapsForProcessorTextures(
            const GrPrimitiveProcessor&, const GrPipeline&,
            const GrTextureProxy* const primitiveProcessorTextures[],
            int numPrimitiveProcessorTextureSets);

    // Flushes state from GrPipeline to GL. Returns false if the state couldn't be set.
    // willDrawPoints must be true if point primitives will be rendered after setting the GL state.
    // If DynamicStateArrays is not null then dynamicStateArraysLength is the number of dynamic
    // state entries in each array.
    bool flushGLState(const GrPrimitiveProcessor&, const GrPipeline&,
                      const GrPipeline::FixedDynamicState*, const GrPipeline::DynamicStateArrays*,
                      int dynamicStateArraysLength, bool willDrawPoints);

    void flushProgram(sk_sp<GrGLProgram>);

    // Version for programs that aren't GrGLProgram.
    void flushProgram(GrGLuint);

    // Sets up vertex/instance attribute pointers and strides.
    void setupGeometry(const GrBuffer* indexBuffer,
                       const GrBuffer* vertexBuffer,
                       int baseVertex,
                       const GrBuffer* instanceBuffer,
                       int baseInstance,
                       GrPrimitiveRestart);

    void flushBlend(const GrXferProcessor::BlendInfo& blendInfo, const GrSwizzle&);

    void onFinishFlush(bool insertedSemaphores) override;

    bool hasExtension(const char* ext) const { return fGLContext->hasExtension(ext); }

    bool copySurfaceAsDraw(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                           GrSurface* src, GrSurfaceOrigin srcOrigin,
                           const SkIRect& srcRect, const SkIPoint& dstPoint);
    void copySurfaceAsCopyTexSubImage(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                                      GrSurface* src, GrSurfaceOrigin srcOrigin,
                                      const SkIRect& srcRect, const SkIPoint& dstPoint);
    bool copySurfaceAsBlitFramebuffer(GrSurface* dst, GrSurfaceOrigin dstOrigin,
                                      GrSurface* src, GrSurfaceOrigin srcOrigin,
                                      const SkIRect& srcRect, const SkIPoint& dstPoint);
    void clearStencilClipAsDraw(const GrFixedClip&, bool insideStencilMask,
                                GrRenderTarget*, GrSurfaceOrigin);

    static bool BlendCoeffReferencesConstant(GrBlendCoeff coeff);

    class ProgramCache : public ::SkNoncopyable {
    public:
        ProgramCache(GrGLGpu* gpu);
        ~ProgramCache();

        void abandon();
        GrGLProgram* refProgram(const GrGLGpu*, const GrPrimitiveProcessor&, const GrPipeline&,
                                bool hasPointSize);

    private:
        // We may actually have kMaxEntries+1 shaders in the GL context because we create a new
        // shader before evicting from the cache.
        static const int kMaxEntries = 128;

        struct Entry;

        // binary search for entry matching desc. returns index into fEntries that matches desc or ~
        // of the index of where it should be inserted.
        int search(const GrProgramDesc& desc) const;

        struct DescHash {
            uint32_t operator()(const GrProgramDesc& desc) const {
                return SkOpts::hash_fn(desc.asKey(), desc.keyLength(), 0);
            }
        };

        SkLRUCache<GrProgramDesc, std::unique_ptr<Entry>, DescHash> fMap;

        GrGLGpu*                    fGpu;
#ifdef PROGRAM_CACHE_STATS
        int                         fTotalRequests;
        int                         fCacheMisses;
        int                         fHashMisses; // cache hit but hash table missed
#endif
    };

    void flushColorWrite(bool writeColor);

    // flushes the scissor. see the note on flushBoundTextureAndParams about
    // flushing the scissor after that function is called.
    void flushScissor(const GrScissorState&,
                      const GrGLIRect& rtViewport,
                      GrSurfaceOrigin rtOrigin);

    // disables the scissor
    void disableScissor();

    void flushWindowRectangles(const GrWindowRectsState&, const GrGLRenderTarget*, GrSurfaceOrigin);
    void disableWindowRectangles();

    // sets a texture unit to use for texture operations other than binding a texture to a program.
    // ensures that such operations don't negatively interact with tracking bound textures.
    void setScratchTextureUnit();

    // The passed bounds contains the render target's color values that will subsequently be
    // written.
    void flushRenderTarget(GrGLRenderTarget*, GrSurfaceOrigin, const SkIRect& bounds);
    // This version has an implicit bounds of the entire render target.
    void flushRenderTarget(GrGLRenderTarget*);
    // This version can be used when the render target's colors will not be written.
    void flushRenderTargetNoColorWrites(GrGLRenderTarget*);

    // Need not be called if flushRenderTarget is used.
    void flushViewport(const GrGLIRect&);

    void flushStencil(const GrStencilSettings&);
    void disableStencil();

    // rt is used only if useHWAA is true.
    void flushHWAAState(GrRenderTarget* rt, bool useHWAA, bool stencilEnabled);

    void flushMinSampleShading(float minSampleShading);

    void flushFramebufferSRGB(bool enable);

    // helper for onCreateTexture and writeTexturePixels
    enum UploadType {
        kNewTexture_UploadType,   // we are creating a new texture
        kWrite_UploadType,        // we are using TexSubImage2D to copy data to an existing texture
    };
    bool uploadTexData(GrPixelConfig texConfig, int texWidth, int texHeight, GrGLenum target,
                       UploadType uploadType, int left, int top, int width, int height,
                       GrPixelConfig dataConfig, const GrMipLevel texels[], int mipLevelCount,
                       GrMipMapsStatus* mipMapsStatus = nullptr);

    bool createRenderTargetObjects(const GrSurfaceDesc&, const GrGLTextureInfo& texInfo,
                                   GrGLRenderTarget::IDDesc*);

    enum TempFBOTarget {
        kSrc_TempFBOTarget,
        kDst_TempFBOTarget
    };

    // Binds a surface as a FBO for copying, reading, or clearing. If the surface already owns an
    // FBO ID then that ID is bound. If not the surface is temporarily bound to a FBO and that FBO
    // is bound. This must be paired with a call to unbindSurfaceFBOForPixelOps().
    void bindSurfaceFBOForPixelOps(GrSurface* surface, GrGLenum fboTarget, GrGLIRect* viewport,
                                   TempFBOTarget tempFBOTarget);

    // Must be called if bindSurfaceFBOForPixelOps was used to bind a surface for copying.
    void unbindTextureFBOForPixelOps(GrGLenum fboTarget, GrSurface* surface);

#ifdef SK_ENABLE_DUMP_GPU
    void onDumpJSON(SkJSONWriter*) const override;
#endif

    bool createCopyProgram(GrTexture* srcTexture);
    bool createMipmapProgram(int progIdx);
    bool createStencilClipClearProgram();
    bool createClearColorProgram();

    std::unique_ptr<GrGLContext> fGLContext;

    // GL program-related state
    ProgramCache*               fProgramCache;

    ///////////////////////////////////////////////////////////////////////////
    ///@name Caching of GL State
    ///@{
    int                         fHWActiveTextureUnitIdx;

    GrGLuint                    fHWProgramID;
    sk_sp<GrGLProgram>          fHWProgram;

    enum TriState {
        kNo_TriState,
        kYes_TriState,
        kUnknown_TriState
    };

    GrGLuint                    fTempSrcFBOID;
    GrGLuint                    fTempDstFBOID;

    GrGLuint                    fStencilClearFBOID;

    // last scissor / viewport scissor state seen by the GL.
    struct {
        TriState    fEnabled;
        GrGLIRect   fRect;
        void invalidate() {
            fEnabled = kUnknown_TriState;
            fRect.invalidate();
        }
    } fHWScissorSettings;

    class {
    public:
        bool valid() const { return kInvalidSurfaceOrigin != fRTOrigin; }
        void invalidate() { fRTOrigin = kInvalidSurfaceOrigin; }
        bool knownDisabled() const { return this->valid() && !fWindowState.enabled(); }
        void setDisabled() {
            fRTOrigin = kTopLeft_GrSurfaceOrigin;
            fWindowState.setDisabled();
        }

        void set(GrSurfaceOrigin rtOrigin, const GrGLIRect& viewport,
                 const GrWindowRectsState& windowState) {
            fRTOrigin = rtOrigin;
            fViewport = viewport;
            fWindowState = windowState;
        }

        bool knownEqualTo(GrSurfaceOrigin rtOrigin, const GrGLIRect& viewport,
                          const GrWindowRectsState& windowState) const {
            if (!this->valid()) {
                return false;
            }
            if (fWindowState.numWindows() && (fRTOrigin != rtOrigin || fViewport != viewport)) {
                return false;
            }
            return fWindowState == windowState;
        }

    private:
        enum { kInvalidSurfaceOrigin = -1 };

        int                  fRTOrigin;
        GrGLIRect            fViewport;
        GrWindowRectsState   fWindowState;
    } fHWWindowRectsState;

    GrGLIRect                   fHWViewport;

    /**
     * Tracks vertex attrib array state.
     */
    class HWVertexArrayState {
    public:
        HWVertexArrayState() : fCoreProfileVertexArray(nullptr) { this->invalidate(); }

        ~HWVertexArrayState() { delete fCoreProfileVertexArray; }

        void invalidate() {
            fBoundVertexArrayIDIsValid = false;
            fDefaultVertexArrayAttribState.invalidate();
            if (fCoreProfileVertexArray) {
                fCoreProfileVertexArray->invalidateCachedState();
            }
        }

        void notifyVertexArrayDelete(GrGLuint id) {
            if (fBoundVertexArrayIDIsValid && fBoundVertexArrayID == id) {
                // Does implicit bind to 0
                fBoundVertexArrayID = 0;
            }
        }

        void setVertexArrayID(GrGLGpu* gpu, GrGLuint arrayID) {
            if (!gpu->glCaps().vertexArrayObjectSupport()) {
                SkASSERT(0 == arrayID);
                return;
            }
            if (!fBoundVertexArrayIDIsValid || arrayID != fBoundVertexArrayID) {
                GR_GL_CALL(gpu->glInterface(), BindVertexArray(arrayID));
                fBoundVertexArrayIDIsValid = true;
                fBoundVertexArrayID = arrayID;
            }
        }

        /**
         * Binds the vertex array that should be used for internal draws, and returns its attrib
         * state. This binds the default VAO (ID=zero) unless we are on a core profile, in which
         * case we use a dummy array instead.
         *
         * If an index buffer is privided, it will be bound to the vertex array. Otherwise the
         * index buffer binding will be left unchanged.
         *
         * The returned GrGLAttribArrayState should be used to set vertex attribute arrays.
         */
        GrGLAttribArrayState* bindInternalVertexArray(GrGLGpu*, const GrBuffer* ibuff = nullptr);

    private:
        GrGLuint             fBoundVertexArrayID;
        bool                 fBoundVertexArrayIDIsValid;

        // We return a non-const pointer to this from bindArrayAndBuffersToDraw when vertex array 0
        // is bound. However, this class is internal to GrGLGpu and this object never leaks out of
        // GrGLGpu.
        GrGLAttribArrayState fDefaultVertexArrayAttribState;

        // This is used when we're using a core profile.
        GrGLVertexArray*     fCoreProfileVertexArray;
    }                                       fHWVertexArrayState;

    struct {
        GrGLenum                fGLTarget;
        GrGpuResource::UniqueID fBoundBufferUniqueID;
        bool                    fBufferZeroKnownBound;

        void invalidate() {
            fBoundBufferUniqueID.makeInvalid();
            fBufferZeroKnownBound = false;
        }
    }                                       fHWBufferState[kGrBufferTypeCount];

    struct {
        GrBlendEquation fEquation;
        GrBlendCoeff    fSrcCoeff;
        GrBlendCoeff    fDstCoeff;
        SkPMColor4f     fConstColor;
        bool            fConstColorValid;
        TriState        fEnabled;

        void invalidate() {
            fEquation = kIllegal_GrBlendEquation;
            fSrcCoeff = kIllegal_GrBlendCoeff;
            fDstCoeff = kIllegal_GrBlendCoeff;
            fConstColorValid = false;
            fEnabled = kUnknown_TriState;
        }
    }                                       fHWBlendState;

    TriState                                fMSAAEnabled;

    GrStencilSettings                       fHWStencilSettings;
    TriState                                fHWStencilTestEnabled;


    TriState                                fHWWriteToColor;
    GrGpuResource::UniqueID                 fHWBoundRenderTargetUniqueID;
    TriState                                fHWSRGBFramebuffer;
    SkTArray<GrGpuResource::UniqueID, true> fHWBoundTextureUniqueIDs;

    GrGLuint fBoundDrawFramebuffer = 0;

    // EXT_raster_multisample.
    TriState                                fHWRasterMultisampleEnabled;
    int                                     fHWNumRasterSamples;
    ///@}

    /** IDs for copy surface program. (3 sampler types) */
    struct {
        GrGLuint    fProgram = 0;
        GrGLint     fTextureUniform = 0;
        GrGLint     fTexCoordXformUniform = 0;
        GrGLint     fPosXformUniform = 0;
    }                                       fCopyPrograms[3];
    sk_sp<GrGLBuffer>                       fCopyProgramArrayBuffer;

    /** IDs for texture mipmap program. (4 filter configurations) */
    struct {
        GrGLuint    fProgram = 0;
        GrGLint     fTextureUniform = 0;
        GrGLint     fTexCoordXformUniform = 0;
    }                                       fMipmapPrograms[4];
    sk_sp<GrGLBuffer>                       fMipmapProgramArrayBuffer;

    GrGLuint                                fStencilClipClearProgram = 0;
    sk_sp<GrGLBuffer>                       fStencilClipClearArrayBuffer;

    /** IDs for clear render target color program. */
    struct {
        GrGLuint    fProgram = 0;
        GrGLint     fColorUniform = 0;
    }                                       fClearColorProgram;
    sk_sp<GrGLBuffer>                       fClearProgramArrayBuffer;

    static int TextureToCopyProgramIdx(GrTexture* texture);

    static int TextureSizeToMipmapProgramIdx(int width, int height) {
        const bool wide = (width > 1) && SkToBool(width & 0x1);
        const bool tall = (height > 1) && SkToBool(height & 0x1);
        return (wide ? 0x2 : 0x0) | (tall ? 0x1 : 0x0);
    }

    float fHWMinSampleShading;
    GrPrimitiveType fLastPrimitiveType;

    class SamplerObjectCache;
    std::unique_ptr<SamplerObjectCache> fSamplerObjectCache;

    std::unique_ptr<GrGLGpuRTCommandBuffer>      fCachedRTCommandBuffer;
    std::unique_ptr<GrGLGpuTextureCommandBuffer> fCachedTexCommandBuffer;

    friend class GrGLPathRendering; // For accessing setTextureUnit.

    typedef GrGpu INHERITED;
};

#endif
