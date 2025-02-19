// Copyright 2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_NEVA_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_NEVA_H_

#include "content/browser/renderer_host/delegated_frame_host.h"

namespace content {

class ClosedKeepAliveWebAppTrigger;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient
    : public neva::DelegatedFrameHostClient {
 public:
  ~DelegatedFrameHostClient() override {}
  virtual bool DelegatedFrameHostIsKeepAliveWebApp() const = 0;
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost : public neva::DelegatedFrameHost {
 public:
  // |should_register_frame_sink_id| flag indicates whether DelegatedFrameHost
  // is responsible for registering the associated FrameSinkId with the
  // compositor or not. This is set only on non-aura platforms, since aura is
  // responsible for doing the appropriate [un]registration.
  DelegatedFrameHost(const viz::FrameSinkId& frame_sink_id,
                     DelegatedFrameHostClient* client,
                     bool should_register_frame_sink_id);
  ~DelegatedFrameHost() override;

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list);
  void WasHidden();
  void WasShown(const viz::LocalSurfaceId& local_surface_id,
                const gfx::Size& dip_size,
                bool record_presentation_time);
  void AttachToCompositor(ui::Compositor* compositor);
  base::WeakPtr<DelegatedFrameHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void DoBackgroundCleanup();

  bool use_aggressive_release_policy_ = false;
  bool deferred_resume_drawing_ = false;
  bool did_first_swap_ = false;
  bool was_hidden_ = false;
  base::CancelableOnceClosure background_cleanup_task_;

  std::unique_ptr<ClosedKeepAliveWebAppTrigger> keep_alive_trigger_;
  base::WeakPtrFactory<DelegatedFrameHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_NEVA_H_
