/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_VIDEO_SEND_STREAM_H_
#define CALL_VIDEO_SEND_STREAM_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "api/rtp_headers.h"
#include "api/rtpparameters.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_encoder_config.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "call/rtp_config.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/frame_callback.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/platform_file.h"

namespace webrtc {

class VideoSendStream {
 public:
  struct StreamStats {
    StreamStats();
    ~StreamStats();

    std::string ToString() const;

    FrameCounts frame_counts;
    bool is_rtx = false;
    bool is_flexfec = false;
    int width = 0;
    int height = 0;
    // TODO(holmer): Move bitrate_bps out to the webrtc::Call layer.
    int total_bitrate_bps = 0;
    int retransmit_bitrate_bps = 0;
    int avg_delay_ms = 0;
    int max_delay_ms = 0;
    StreamDataCounters rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;
    RtcpStatistics rtcp_stats;
  };

  struct Stats {
    Stats();
    ~Stats();
    std::string ToString(int64_t time_ms) const;
    std::string encoder_implementation_name = "unknown";
    int input_frame_rate = 0;
    int encode_frame_rate = 0;
    int avg_encode_time_ms = 0;
    int encode_usage_percent = 0;
    uint32_t frames_encoded = 0;
    uint32_t frames_dropped_by_capturer = 0;
    uint32_t frames_dropped_by_encoder_queue = 0;
    uint32_t frames_dropped_by_rate_limiter = 0;
    uint32_t frames_dropped_by_encoder = 0;
    rtc::Optional<uint64_t> qp_sum;
    // Bitrate the encoder is currently configured to use due to bandwidth
    // limitations.
    int target_media_bitrate_bps = 0;
    // Bitrate the encoder is actually producing.
    int media_bitrate_bps = 0;
    // Media bitrate this VideoSendStream is configured to prefer if there are
    // no bandwidth limitations.
    int preferred_media_bitrate_bps = 0;
    bool suspended = false;
    bool bw_limited_resolution = false;
    bool cpu_limited_resolution = false;
    bool bw_limited_framerate = false;
    bool cpu_limited_framerate = false;
    // Total number of times resolution as been requested to be changed due to
    // CPU/quality adaptation.
    int number_of_cpu_adapt_changes = 0;
    int number_of_quality_adapt_changes = 0;
    bool has_entered_low_resolution = false;
    std::map<uint32_t, StreamStats> substreams;
    webrtc::VideoContentType content_type =
        webrtc::VideoContentType::UNSPECIFIED;
    uint32_t huge_frames_sent = 0;
  };

  struct Config {
   public:
    Config() = delete;
    Config(Config&&);
    explicit Config(Transport* send_transport);

    Config& operator=(Config&&);
    Config& operator=(const Config&) = delete;

    ~Config();

    // Mostly used by tests.  Avoid creating copies if you can.
    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    struct EncoderSettings {
      EncoderSettings() = default;
      std::string ToString() const;

      // Enables the new method to estimate the cpu load from encoding, used for
      // cpu adaptation.
      bool experiment_cpu_load_estimator = false;

      // Enables hardware VAAPI VP8 encoding if supported by the provided
      // VideoEncoderFactory.
      // TODO(ilnik): remove this when VAAPI VP8 experiment is over.
      bool experiment_vaapi_vp8_hw_encoding = false;

      // Ownership stays with WebrtcVideoEngine (delegated from PeerConnection).
      VideoEncoderFactory* encoder_factory = nullptr;
    } encoder_settings;

    static const size_t kDefaultMaxPacketSize = 1500 - 40;  // TCP over IPv4.
    struct Rtp {
      Rtp();
      Rtp(const Rtp&);
      ~Rtp();
      std::string ToString() const;

      std::vector<uint32_t> ssrcs;

      // The value to send in the MID RTP header extension if the extension is
      // included in the list of extensions.
      std::string mid;

      // See RtcpMode for description.
      RtcpMode rtcp_mode = RtcpMode::kCompound;

      // Max RTP packet size delivered to send transport from VideoEngine.
      size_t max_packet_size = kDefaultMaxPacketSize;

      // RTP header extensions to use for this send stream.
      std::vector<RtpExtension> extensions;

      // TODO(nisse): For now, these are fixed, but we'd like to support
      // changing codec without recreating the VideoSendStream. Then these
      // fields must be removed, and association between payload type and codec
      // must move above the per-stream level. Ownership could be with
      // RtpTransportControllerSend, with a reference from PayloadRouter, where
      // the latter would be responsible for mapping the codec type of encoded
      // images to the right payload type.
      std::string payload_name;
      int payload_type = -1;

      // See NackConfig for description.
      NackConfig nack;

      // See UlpfecConfig for description.
      UlpfecConfig ulpfec;

      struct Flexfec {
        Flexfec();
        Flexfec(const Flexfec&);
        ~Flexfec();
        // Payload type of FlexFEC. Set to -1 to disable sending FlexFEC.
        int payload_type = -1;

        // SSRC of FlexFEC stream.
        uint32_t ssrc = 0;

        // Vector containing a single element, corresponding to the SSRC of the
        // media stream being protected by this FlexFEC stream.
        // The vector MUST have size 1.
        //
        // TODO(brandtr): Update comment above when we support
        // multistream protection.
        std::vector<uint32_t> protected_media_ssrcs;
      } flexfec;

      // Settings for RTP retransmission payload format, see RFC 4588 for
      // details.
      struct Rtx {
        Rtx();
        Rtx(const Rtx&);
        ~Rtx();
        std::string ToString() const;
        // SSRCs to use for the RTX streams.
        std::vector<uint32_t> ssrcs;

        // Payload type to use for the RTX stream.
        int payload_type = -1;
      } rtx;

      // RTCP CNAME, see RFC 3550.
      std::string c_name;
    } rtp;

    struct Rtcp {
      Rtcp();
      Rtcp(const Rtcp&);
      ~Rtcp();
      std::string ToString() const;

      // Time interval between RTCP report for video
      int64_t video_report_interval_ms = 1000;
      // Time interval between RTCP report for audio
      int64_t audio_report_interval_ms = 5000;
    } rtcp;

    // Transport for outgoing packets.
    Transport* send_transport = nullptr;

    // Called for each I420 frame before encoding the frame. Can be used for
    // effects, snapshots etc. 'nullptr' disables the callback.
    rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback = nullptr;

    // Called for each encoded frame, e.g. used for file storage. 'nullptr'
    // disables the callback. Also measures timing and passes the time
    // spent on encoding. This timing will not fire if encoding takes longer
    // than the measuring window, since the sample data will have been dropped.
    EncodedFrameObserver* post_encode_callback = nullptr;

    // Expected delay needed by the renderer, i.e. the frame will be delivered
    // this many milliseconds, if possible, earlier than expected render time.
    // Only valid if |local_renderer| is set.
    int render_delay_ms = 0;

    // Target delay in milliseconds. A positive value indicates this stream is
    // used for streaming instead of a real-time call.
    int target_delay_ms = 0;

    // True if the stream should be suspended when the available bitrate fall
    // below the minimum configured bitrate. If this variable is false, the
    // stream may send at a rate higher than the estimated available bitrate.
    bool suspend_below_min_bitrate = false;

    // Enables periodic bandwidth probing in application-limited region.
    bool periodic_alr_bandwidth_probing = false;

    // Track ID as specified during track creation.
    std::string track_id;

   private:
    // Access to the copy constructor is private to force use of the Copy()
    // method for those exceptional cases where we do use it.
    Config(const Config&);
  };

  // Updates the sending state for all simulcast layers that the video send
  // stream owns. This can mean updating the activity one or for multiple
  // layers. The ordering of active layers is the order in which the
  // rtp modules are stored in the VideoSendStream.
  // Note: This starts stream activity if it is inactive and one of the layers
  // is active. This stops stream activity if it is active and all layers are
  // inactive.
  virtual void UpdateActiveSimulcastLayers(
      const std::vector<bool> active_layers) = 0;

  // Starts stream activity.
  // When a stream is active, it can receive, process and deliver packets.
  virtual void Start() = 0;
  // Stops stream activity.
  // When a stream is stopped, it can't receive, process or deliver packets.
  virtual void Stop() = 0;

  virtual void SetSource(
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  // Set which streams to send. Must have at least as many SSRCs as configured
  // in the config. Encoder settings are passed on to the encoder instance along
  // with the VideoStream settings.
  virtual void ReconfigureVideoEncoder(VideoEncoderConfig config) = 0;

  virtual Stats GetStats() = 0;

  // Takes ownership of each file, is responsible for closing them later.
  // Calling this method will close and finalize any current logs.
  // Some codecs produce multiple streams (VP8 only at present), each of these
  // streams will log to a separate file. kMaxSimulcastStreams in common_types.h
  // gives the max number of such streams. If there is no file for a stream, or
  // the file is rtc::kInvalidPlatformFileValue, frames from that stream will
  // not be logged.
  // If a frame to be written would make the log too large the write fails and
  // the log is closed and finalized. A |byte_limit| of 0 means no limit.
  virtual void EnableEncodedFrameRecording(
      const std::vector<rtc::PlatformFile>& files,
      size_t byte_limit) = 0;
  inline void DisableEncodedFrameRecording() {
    EnableEncodedFrameRecording(std::vector<rtc::PlatformFile>(), 0);
  }

 protected:
  virtual ~VideoSendStream() {}
};

}  // namespace webrtc

#endif  // CALL_VIDEO_SEND_STREAM_H_
