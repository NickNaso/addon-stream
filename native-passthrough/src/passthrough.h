/*******************************************************************************
 * Copyright (c) 2026 Nicola Del Gobbo
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the license at http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
 * IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
 * MERCHANTABLITY OR NON-INFRINGEMENT.
 *
 * See the Apache Version 2.0 License for specific language governing
 * permissions and limitations under the License.
 *
 * Contributors - initial API implementation:
 * Nicola Del Gobbo <nicoladelgobbo@gmail.com>
 ******************************************************************************/

#ifndef NATIVE_PASSTHROUGH_H
#define NATIVE_PASSTHROUGH_H

#include <napi.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <vector>

using Chunk = std::vector<uint8_t>;

// Native side of the NativePassThrough stream.
//
// Chunks written from JavaScript are copied into a bounded queue and processed
// one at a time on the libuv thread pool, so the event loop is never blocked
// and the output keeps the order of the input. Flow control works in both
// directions:
//  - write() returns false once the queue reaches the high water mark, and
//    onDrain() is called when there is room again (writable side);
//  - when onData() returns false processing stops until resume() is called
//    (readable side).
//
// The handle calls back into JavaScript through the onData, onDrain, onEnd and
// onError properties of its own JS object, looked up on every call. Keeping
// them in persistent references would create a cycle through a GC root and
// the stream would never be collected.
//
// All the state is touched only on the main thread: the worker thread sees
// nothing but the chunk it is processing.
class PassThrough : public Napi::ObjectWrap<PassThrough> {
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
  PassThrough(const Napi::CallbackInfo& info);

  // Transformation applied to every chunk. It runs on a worker thread, so it
  // must not touch any JavaScript value. Throw a std::exception to fail the
  // stream.
  static Chunk Process(Chunk chunk);

 private:
  friend class ProcessWorker;

  Napi::Value Write(const Napi::CallbackInfo& info);
  void Resume(const Napi::CallbackInfo& info);
  void End(const Napi::CallbackInfo& info);
  void Destroy(const Napi::CallbackInfo& info);
  Napi::Value GetBufferedBytes(const Napi::CallbackInfo& info);

  void ProcessNext();
  void OnProcessed(size_t input_size, Chunk output);
  void OnFailed(const Napi::Error& error);
  bool HasCapacity() const;
  Napi::Value CallJs(const char* name,
                     const std::initializer_list<napi_value>& args = {});

  std::deque<Chunk> queue_;
  size_t buffered_bytes_ = 0;  // Queued plus being processed.
  size_t high_water_mark_ = 0;
  bool processing_ = false;
  bool paused_ = false;
  bool read_requested_ = false;
  bool need_drain_ = false;
  bool ending_ = false;
  bool ended_ = false;
  bool destroyed_ = false;
};

#endif  // NATIVE_PASSTHROUGH_H
