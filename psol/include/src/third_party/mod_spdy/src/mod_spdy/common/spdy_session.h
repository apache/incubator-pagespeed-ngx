// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MOD_SPDY_COMMON_SPDY_SESSION_H_
#define MOD_SPDY_COMMON_SPDY_SESSION_H_

#include <map>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "mod_spdy/common/executor.h"
#include "mod_spdy/common/spdy_frame_priority_queue.h"
#include "mod_spdy/common/spdy_server_push_interface.h"
#include "mod_spdy/common/spdy_stream.h"
#include "net/instaweb/util/public/function.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_protocol.h"

namespace mod_spdy {

class Executor;
class SpdySessionIO;
class SpdyServerConfig;
class SpdyStreamTaskFactory;

// Represents a SPDY session with a client.  Given an Executor for processing
// individual SPDY streams, and a SpdySessionIO for communicating with the
// client (sending and receiving frames), this class takes care of implementing
// the SPDY protocol and responding correctly to various situations.
class SpdySession : public net::BufferedSpdyFramerVisitorInterface,
                    public SpdyServerPushInterface {
 public:
  // The SpdySession does _not_ take ownership of any of these arguments.
  SpdySession(int spdy_version,
              const SpdyServerConfig* config,
              SpdySessionIO* session_io,
              SpdyStreamTaskFactory* task_factory,
              Executor* executor);
  virtual ~SpdySession();

  // What SPDY version is being used for this session?
  // TODO(mdsteele): This method should be const, but it isn't beacuse
  //   BufferedSpdyFramer::protocol_version() isn't const, for no reason.
  int spdy_version() { return framer_.protocol_version(); }

  // Process the session; don't return until the session is finished.
  void Run();

  // BufferedSpdyFramerVisitorInterface methods:
  virtual void OnError(net::SpdyFramer::SpdyError error_code);
  virtual void OnStreamError(net::SpdyStreamId stream_id,
                             const std::string& description);
  virtual void OnSynStream(net::SpdyStreamId stream_id,
                           net::SpdyStreamId associated_stream_id,
                           net::SpdyPriority priority,
                           uint8 credential_slot,
                           bool fin,
                           bool unidirectional,
                           const net::SpdyHeaderBlock& headers);
  virtual void OnSynReply(net::SpdyStreamId stream_id,
                          bool fin,
                          const net::SpdyHeaderBlock& headers);
  virtual void OnHeaders(net::SpdyStreamId stream_id,
                         bool fin,
                         const net::SpdyHeaderBlock& headers);
  virtual void OnStreamFrameData(net::SpdyStreamId stream_id,
                                 const char* data, size_t length,
                                 net::SpdyDataFlags flags);
  virtual void OnSetting(net::SpdySettingsIds id, uint8 flags, uint32 value);
  virtual void OnPing(uint32 unique_id);
  virtual void OnRstStream(net::SpdyStreamId stream_id,
                           net::SpdyStatusCodes status);
  virtual void OnGoAway(net::SpdyStreamId last_accepted_stream_id,
                        net::SpdyGoAwayStatus status);
  virtual void OnWindowUpdate(net::SpdyStreamId stream_id,
                              int delta_window_size);
  virtual void OnControlFrameCompressed(
      const net::SpdyControlFrame& uncompressed_frame,
      const net::SpdyControlFrame& compressed_frame);

  // SpdyServerPushInterface methods:
  // Initiate a SPDY server push, roughly by pretending that the client sent a
  // SYN_STREAM with the given headers.  To repeat: the headers argument is
  // _not_ the headers that the server will send to the client, but rather the
  // headers to _pretend_ that the client sent to the server.  Requires that
  // spdy_version() >= 3.
  // Note that unlike most other methods of this class, StartServerPush may be
  // called by stream threads, not just by the connection thread.
  virtual SpdyServerPushInterface::PushStatus StartServerPush(
      net::SpdyStreamId associated_stream_id,
      int32 server_push_depth,
      net::SpdyPriority priority,
      const net::SpdyHeaderBlock& request_headers);

 private:
  // A helper class for wrapping tasks returned by
  // SpdyStreamTaskFactory::NewStreamTask().  Running or cancelling this task
  // simply runs/cancels the wrapped task; however, this object also keeps a
  // SpdyStream object, and on deletion, this will remove itself from the
  // SpdySession's list of active streams.
  class StreamTaskWrapper : public net_instaweb::Function {
   public:
    // This constructor, called by the main connection thread, will call
    // task_factory_->NewStreamTask() to produce the wrapped task.
    StreamTaskWrapper(SpdySession* spdy_session,
                      net::SpdyStreamId stream_id,
                      net::SpdyStreamId associated_stream_id,
                      int32 server_push_depth,
                      net::SpdyPriority priority);
    virtual ~StreamTaskWrapper();

    SpdyStream* stream() { return &stream_; }

   protected:
    // net_instaweb::Function methods (our implementations of these simply
    // run/cancel the wrapped subtask):
    virtual void Run();
    virtual void Cancel();

   private:
    SpdySession* const spdy_session_;
    SpdyStream stream_;
    net_instaweb::Function* const subtask_;

    DISALLOW_COPY_AND_ASSIGN(StreamTaskWrapper);
  };

  // Helper class for keeping track of active stream tasks, and separately
  // tracking the number of active client/server-initiated streams.  This class
  // is not thread-safe without external synchronization, so it is used below
  // along with a separate mutex.
  class SpdyStreamMap {
   public:
    SpdyStreamMap();
    ~SpdyStreamMap();

    // Determine whether there are no currently active streams.
    bool IsEmpty();
    // Get the number of currently active streams created by the client or
    // server, respectively.
    size_t NumActiveClientStreams();
    size_t NumActivePushStreams();
    // Determine if a particular stream ID is currently active.
    bool IsStreamActive(net::SpdyStreamId stream_id);
    // Get the specified stream object, or NULL if the stream is inactive.
    SpdyStream* GetStream(net::SpdyStreamId stream_id);
    // Add a new stream.  Requires that the stream ID is currently inactive.
    void AddStreamTask(StreamTaskWrapper* task);
    // Remove a stream task.  Requires that the stream is currently active.
    void RemoveStreamTask(StreamTaskWrapper* task);
    // Adjust the output window size of all active streams by the same delta.
    void AdjustAllOutputWindowSizes(int32 delta);
    // Abort all streams in the map.  Note that this won't immediately empty
    // the map (the tasks still have to shut down).
    void AbortAllSilently();

   private:
    typedef std::map<net::SpdyStreamId, StreamTaskWrapper*> TaskMap;
    TaskMap tasks_;
    size_t num_active_push_streams_;

    DISALLOW_COPY_AND_ASSIGN(SpdyStreamMap);
  };

  // Validate and set the per-stream initial flow-control window size to the
  // new value.  Must be using SPDY v3 or later to call this method.
  void SetInitialWindowSize(uint32 new_init_window_size);

  // Send a single SPDY frame to the client, compressing it first if necessary.
  // Stop the session if the connection turns out to be closed.  This method
  // takes ownership of the passed frame and will delete it.
  void SendFrame(const net::SpdyFrame* frame);
  // Send the frame as-is (without taking ownership).  Stop the session if the
  // connection turns out to be closed.
  void SendFrameRaw(const net::SpdyFrame& frame);

  // Immediately send a GOAWAY frame to the client with the given status,
  // unless we've already sent one.  This also prevents us from creating any
  // new streams, so calling this is the best way to shut the session down
  // gracefully; once all streams have finished normally and no new ones can be
  // created, the session will shut itself down.
  void SendGoAwayFrame(net::SpdyGoAwayStatus status);
  // Enqueue a RST_STREAM frame for the given stream ID.  Note that this does
  // not abort the stream if it exists; for that, use AbortStream().
  void SendRstStreamFrame(net::SpdyStreamId stream_id,
                          net::SpdyStatusCodes status);
  // Immediately send our SETTINGS frame, with values based on our
  // SpdyServerConfig object.  This should be done exactly once, at session
  // start.
  void SendSettingsFrame();

  // Close down the whole session immediately.  Abort all active streams, and
  // then block until all stream threads have shut down.
  void StopSession();
  // Abort the stream without sending anything to the client.
  void AbortStreamSilently(net::SpdyStreamId stream_id);
  // Send a RST_STREAM frame and then abort the stream.
  void AbortStream(net::SpdyStreamId stream_id, net::SpdyStatusCodes status);

  // Remove the given StreamTaskWrapper object from the stream map.  This is
  // the only other method of this class, aside from StartServerPush, that
  // might be called from another thread.  (Specifically, it is called by the
  // StreamTaskWrapper destructor, which is called by the executor).
  void RemoveStreamTask(StreamTaskWrapper* stream_data);

  // Grab the stream_map_lock_ and check if stream_map_ is empty.
  bool StreamMapIsEmpty();

  // These fields are accessed only by the main connection thread, so they need
  // not be protected by a lock:
  const SpdyServerConfig* const config_;
  SpdySessionIO* const session_io_;
  SpdyStreamTaskFactory* const task_factory_;
  Executor* const executor_;
  net::BufferedSpdyFramer framer_;
  bool session_stopped_;  // StopSession() has been called
  bool already_sent_goaway_;  // GOAWAY frame has been sent
  net::SpdyStreamId last_client_stream_id_;
  int32 initial_window_size_;  // per-stream initial flow-control window size
  uint32 max_concurrent_pushes_;  // max number of active server pushes at once

  // The stream map must be protected by a lock, because each stream thread
  // will remove itself from the map (by calling RemoveStreamTask) when the
  // stream closes.  You MUST hold the lock to use the stream_map_ OR to use
  // any of the StreamTaskWrapper or SpdyStream objects contained therein
  // (e.g. to post a frame to the stream), otherwise the stream object may be
  // deleted by another thread while you're using it.  You should NOT be
  // holding the lock when you e.g. send a frame to the client, as that may
  // block for a long time.
  base::Lock stream_map_lock_;
  SpdyStreamMap stream_map_;
  // These fields are also protected by the stream_map_lock_; they are used for
  // controlling server pushes, which can be initiated by stream threads as
  // well as by the connection thread.  We could use a separate lock for these,
  // but right now we probably don't need that much locking granularity.
  net::SpdyStreamId last_server_push_stream_id_;
  bool received_goaway_;  // we've received a GOAWAY frame from the client

  // The output queue is also shared between all stream threads, but its class
  // is thread-safe, so it doesn't need additional synchronization.
  SpdyFramePriorityQueue output_queue_;

  DISALLOW_COPY_AND_ASSIGN(SpdySession);
};

}  // namespace mod_spdy

#endif  // MOD_SPDY_COMMON_SPDY_SESSION_H_
