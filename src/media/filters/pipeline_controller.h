// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_PIPELINE_CONTROLLER_H_
#define MEDIA_FILTERS_PIPELINE_CONTROLLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline.h"
#include "media/base/renderer.h"

#if defined(USE_NEVA_MEDIA)
#include "media/base/neva/media_platform_api.h"
#endif

namespace media {
class Demuxer;

// PipelineController wraps a Pipeline to expose the one-at-a-time operations
// (Seek(), Suspend(), and Resume()) with a simpler API. Internally it tracks
// pending operations and dispatches them when possible. Duplicate requests
// (such as seeking twice to the same time) may be elided.
//
// TODO(sandersd/tguilbert):
//   - Expose an operation that replaces the Renderer (via Suspend/Resume).
//   - Expose an operation that replaces the Demuxer (via Start/Stop). This will
//     also implicitly replace the Renderer.
//   - Block invalid calls after an error occurs.
class MEDIA_EXPORT PipelineController {
 public:
  enum class State {
    STOPPED,
    STARTING,
    PLAYING,
    PLAYING_OR_SUSPENDED,
    SEEKING,
    SWITCHING_TRACKS,
    SUSPENDING,
    SUSPENDED,
    RESUMING,
  };

  using RendererFactoryCB = base::Callback<std::unique_ptr<Renderer>(void)>;
  using SeekedCB = base::Callback<void(bool time_updated)>;
  using SuspendedCB = base::Callback<void()>;
  using BeforeResumeCB = base::Callback<void()>;
  using ResumedCB = base::Callback<void()>;

  // Construct a PipelineController wrapping |pipeline_|.
  // The callbacks are:
  //   - |renderer_factory_cb| is called by PipelineController to create new
  //     renderers when starting and resuming.
  //   - |seeked_cb| is called upon reaching a stable state if a seek occured.
  //   - |suspended_cb| is called immediately after suspending.
  //   - |before_resume_cb| is called immediately before resuming.
  //   - |resumed_cb| is called immediately after resuming.
  //   - |error_cb| is called if any operation on |pipeline_| does not result
  //     in PIPELINE_OK or its error callback is called.
  PipelineController(std::unique_ptr<Pipeline> pipeline,
                     const RendererFactoryCB& renderer_factory_cb,
                     const SeekedCB& seeked_cb,
                     const SuspendedCB& suspended_cb,
                     const BeforeResumeCB& before_resume_cb,
                     const ResumedCB& resumed_cb,
                     const PipelineStatusCB& error_cb);
  ~PipelineController();

  // Start |pipeline_|. |demuxer| will be retained and StartWaitingForSeek()/
  // CancelPendingSeek() will be issued to it as necessary.
  //
  // When |is_streaming| is true, Resume() will always start at the
  // beginning of the stream, rather than attempting to seek to the current
  // time.
  //
  // When |is_static| is true, seeks to the current time may be elided.
  // Otherwise it is assumed that the media data may have changed.
  //
  // The remaining parameters are just passed directly to pipeline_.Start().
  void Start(Pipeline::StartType start_type,
             Demuxer* demuxer,
             Pipeline::Client* client,
             bool is_streaming,
             bool is_static);

  // Request a seek to |time|. If |time_updated| is true, then the eventual
  // |seeked_cb| callback will also have |time_updated| set to true; it
  // indicates that the seek was requested by Blink and a time update is
  // expected so that Blink can fire the seeked event.
  //
  // Note: This will not resume the pipeline if it is in the suspended state; a
  // call to Resume() is required. |seeked_cb_| will not be called until the
  // later Resume() completes. The intention is to avoid unnecessary wake-ups
  // for suspended players.
  void Seek(base::TimeDelta time, bool time_updated);

  // Request that |pipeline_| be suspended. This is a no-op if |pipeline_| has
  // been suspended.
  void Suspend();

  // Request that |pipeline_| be resumed. This is a no-op if |pipeline_| has not
  // been suspended.
  void Resume();

  // Returns true if the current state is stable. This means that |state_| is
  // PLAYING and there are no pending operations. Requests are processed
  // immediately when the state is stable, otherwise they are queued.
  //
  // Exceptions to the above:
  //   - Start() is processed immediately while in the CREATED state.
  //   - Resume() is processed immediately while in the SUSPENDED state.
  bool IsStable();

  // Returns true if the current target state is suspended.
  bool IsSuspended();

  // Returns true if Seek() was called and there is a seek operation which has
  // not yet completed.
  bool IsPendingSeek();

  // Returns true if |pipeline_| is suspended.
  bool IsPipelineSuspended();

  // Subset of the Pipeline interface directly exposing |pipeline_|.
  void Stop();
  bool IsPipelineRunning() const;
  double GetPlaybackRate() const;
  void SetPlaybackRate(double playback_rate);
  float GetVolume() const;
  void SetVolume(float volume);
  base::TimeDelta GetMediaTime() const;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const;
  base::TimeDelta GetMediaDuration() const;
  bool DidLoadingProgress();
  PipelineStatistics GetStatistics() const;
  void SetCdm(CdmContext* cdm_context, const CdmAttachedCB& cdm_attached_cb);
  void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& enabled_track_ids);
  void OnSelectedVideoTrackChanged(
      base::Optional<MediaTrack::Id> selected_track_id);

#if defined(USE_NEVA_MEDIA)
  void SetMediaPlatformAPI(
      const scoped_refptr<MediaPlatformAPI>& media_platform_api);
#endif

  // Used to fire the OnTrackChangeComplete function which is captured in a
  // OnceCallback, and doesn't play nicely with gmock.
  void FireOnTrackChangeCompleteForTesting(State set_to);

 private:
  // Attempts to make progress from the current state to the target state.
  void Dispatch();

  // PipelineStaus callback that also carries the target state.
  void OnPipelineStatus(State state, PipelineStatus pipeline_status);

  void OnTrackChangeComplete(State previous_state);

  // The Pipeline we are managing state for.
  std::unique_ptr<Pipeline> pipeline_;

  // Factory for Renderers, used for Start() and Resume().
  RendererFactoryCB renderer_factory_cb_;

  // Called after seeks (which includes Start()) upon reaching a stable state.
  // Multiple seeks result in only one callback if no stable state occurs
  // between them.
  SeekedCB seeked_cb_;

  // Called immediately when |pipeline_| completes a suspend operation.
  SuspendedCB suspended_cb_;

  // Called immediately before |pipeline_| starts a resume operation.
  ResumedCB before_resume_cb_;

  // Called immediately when |pipeline_| completes a resume operation.
  ResumedCB resumed_cb_;

  // Called immediately when any operation on |pipeline_| results in an error.
  PipelineStatusCB error_cb_;

  // State for handling StartWaitingForSeek()/CancelPendingSeek().
  Demuxer* demuxer_ = nullptr;
  bool waiting_for_seek_ = false;

  // When true, Resume() will start at time zero instead of seeking to the
  // current time.
  bool is_streaming_ = false;

  // When true, seeking to the current time may be elided.
  bool is_static_ = true;

  // Tracks the current state of |pipeline_|.
  State state_ = State::STOPPED;

  // Indicates that a seek has occurred. When set, a seeked callback will be
  // issued at the next stable state.
  bool pending_seeked_cb_ = false;

  // Indicates that a seek has occurred from an explicit call to Seek().
  bool pending_seek_except_start_ = false;

  // Indicates that time has been changed by a seek, which will be reported at
  // the next seeked callback.
  bool pending_time_updated_ = false;

  // The target time of the active seek; valid while SEEKING or RESUMING.
  base::TimeDelta seek_time_;

  // Target state which we will work to achieve.
  bool pending_seek_ = false;
  bool pending_suspend_ = false;
  bool pending_resume_ = false;
  bool pending_audio_track_change_ = false;
  bool pending_video_track_change_ = false;

  // |pending_seek_time_| is only valid when |pending_seek_| is true.
  // |pending_track_change_type_| is only valid when |pending_track_change_|.
  // |pending_audio_track_change_ids_| is only valid when
  //   |pending_audio_track_change_|.
  // |pending_video_track_change_id_| is only valid when
  //   |pending_video_track_change_|.
  base::TimeDelta pending_seek_time_;
  std::vector<MediaTrack::Id> pending_audio_track_change_ids_;
  base::Optional<MediaTrack::Id> pending_video_track_change_id_;

  // Set to true during Start(). Indicates that |seeked_cb_| must be fired once
  // we've completed startup.
  bool pending_startup_ = false;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PipelineController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipelineController);
};

}  // namespace media

#endif  // MEDIA_FILTERS_PIPELINE_CONTROLLER_H_
