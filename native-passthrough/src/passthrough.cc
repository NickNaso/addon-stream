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

#include "passthrough.h"

#include <utility>

namespace {

// Moves the bytes into a JS Buffer without copying them, or falls back to a
// copy when the runtime does not allow external buffers.
Napi::Buffer<uint8_t> ToBuffer(Napi::Env env, Chunk chunk) {
  Chunk* data = new Chunk(std::move(chunk));
  return Napi::Buffer<uint8_t>::NewOrCopy(
      env, data->data(), data->size(),
      [](Napi::Env /*env*/, uint8_t* /*bytes*/, Chunk* hint) { delete hint; },
      data);
}

}  // namespace

// Processes a single chunk on the libuv thread pool. Execute() runs on a
// worker thread, OnOK() and OnError() back on the main thread.
class ProcessWorker : public Napi::AsyncWorker {
 public:
  ProcessWorker(PassThrough* stream, Chunk chunk)
  : Napi::AsyncWorker(stream->Env(), "NativePassThrough"),
    stream_(stream),
    // Keeps the JS object, and so the native instance, alive until the
    // worker has completed and is deleted.
    stream_ref_(Napi::Persistent(stream->Value())),
    input_size_(chunk.size()),
    chunk_(std::move(chunk)) {}

  void Execute() override {
    chunk_ = PassThrough::Process(std::move(chunk_));
  }

  void OnOK() override {
    stream_->OnProcessed(input_size_, std::move(chunk_));
  }

  void OnError(const Napi::Error& error) override {
    stream_->OnFailed(error);
  }

 private:
  PassThrough* stream_;
  Napi::ObjectReference stream_ref_;
  size_t input_size_;
  Chunk chunk_;
};

Napi::Object PassThrough::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "PassThrough", {
    InstanceMethod("write", &PassThrough::Write),
    InstanceMethod("resume", &PassThrough::Resume),
    InstanceMethod("end", &PassThrough::End),
    InstanceMethod("destroy", &PassThrough::Destroy),
    InstanceAccessor("bufferedBytes", &PassThrough::GetBufferedBytes, nullptr)
  });

  exports.Set("PassThrough", func);
  return exports;
}

PassThrough::PassThrough(const Napi::CallbackInfo& info)
: Napi::ObjectWrap<PassThrough>(info) {
  if (info.Length() < 1 || !info[0].IsNumber()) {
    throw Napi::TypeError::New(info.Env(), "highWaterMark must be a number");
  }
  int64_t high_water_mark = info[0].As<Napi::Number>().Int64Value();
  if (high_water_mark < 0) {
    throw Napi::RangeError::New(info.Env(), "highWaterMark must be >= 0");
  }
  high_water_mark_ = static_cast<size_t>(high_water_mark);
}

Chunk PassThrough::Process(Chunk chunk) {
  // A passthrough hands the bytes back untouched. This is the place for the
  // real work: compression, encryption, parsing, calls to a blocking library.
  return chunk;
}

Napi::Value PassThrough::Write(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsBuffer()) {
    throw Napi::TypeError::New(env, "The chunk must be a Buffer");
  }
  if (ending_ || destroyed_) {
    throw Napi::Error::New(env, "write after end");
  }
  Napi::Buffer<uint8_t> buffer = info[0].As<Napi::Buffer<uint8_t>>();
  if (buffer.Length() > 0) {
    // Copy the bytes: once the write callback runs JavaScript may reuse the
    // buffer, while the chunk can still be waiting in the queue.
    queue_.emplace_back(buffer.Data(), buffer.Data() + buffer.Length());
    buffered_bytes_ += buffer.Length();
    ProcessNext();
  }
  bool has_capacity = HasCapacity();
  if (!has_capacity) {
    need_drain_ = true;
  }
  return Napi::Boolean::New(env, has_capacity);
}

void PassThrough::Resume(const Napi::CallbackInfo& /*info*/) {
  read_requested_ = true;
  paused_ = false;
  ProcessNext();
}

void PassThrough::End(const Napi::CallbackInfo& /*info*/) {
  if (ending_ || destroyed_) {
    return;
  }
  ending_ = true;
  ProcessNext();
}

void PassThrough::Destroy(const Napi::CallbackInfo& /*info*/) {
  // A chunk already handed to the thread pool cannot be stopped: its result
  // is dropped in OnProcessed().
  destroyed_ = true;
  queue_.clear();
  buffered_bytes_ = 0;
}

Napi::Value PassThrough::GetBufferedBytes(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), static_cast<double>(buffered_bytes_));
}

void PassThrough::ProcessNext() {
  if (destroyed_ || processing_) {
    return;
  }
  if (queue_.empty()) {
    // Everything written has been pushed: the readable side can end, even if
    // the consumer is not reading right now.
    if (ending_ && !ended_) {
      ended_ = true;
      CallJs("onEnd");
    }
    return;
  }
  if (paused_) {
    return;
  }
  ProcessWorker* worker = new ProcessWorker(this, std::move(queue_.front()));
  queue_.pop_front();
  processing_ = true;
  worker->Queue();
}

void PassThrough::OnProcessed(size_t input_size, Chunk output) {
  processing_ = false;
  if (destroyed_) {
    return;
  }
  buffered_bytes_ -= input_size;

  if (!output.empty()) {
    read_requested_ = false;
    bool more = CallJs("onData", {ToBuffer(Env(), std::move(output))})
                    .ToBoolean();
    if (destroyed_) {
      return;
    }
    // onData() returns the result of readable.push(): false means that the
    // readable buffer is full, so wait for _read() to call resume(). Unless
    // _read() already asked for more data while onData() was running.
    if (!more && !read_requested_) {
      paused_ = true;
    }
  }

  if (need_drain_ && HasCapacity()) {
    need_drain_ = false;
    CallJs("onDrain");
    if (destroyed_) {
      return;
    }
  }

  ProcessNext();
}

void PassThrough::OnFailed(const Napi::Error& error) {
  processing_ = false;
  if (destroyed_) {
    return;
  }
  // The stream cannot go on after a failed chunk: drop everything and let
  // JavaScript destroy the stream with the error.
  destroyed_ = true;
  queue_.clear();
  buffered_bytes_ = 0;
  CallJs("onError", {error.Value()});
}

bool PassThrough::HasCapacity() const {
  // With a high water mark of 0 the queue still takes one chunk at a time.
  return buffered_bytes_ < high_water_mark_ || buffered_bytes_ == 0;
}

Napi::Value PassThrough::CallJs(
    const char* name, const std::initializer_list<napi_value>& args) {
  Napi::Object self = Value();
  Napi::Value handler = self.Get(name);
  if (!handler.IsFunction()) {
    return Env().Undefined();
  }
  return handler.As<Napi::Function>().Call(self, args);
}
