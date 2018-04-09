/*******************************************************************************
 * Copyright (c) 2018 Nicola Del Gobbo
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
#include <iostream>

Napi::FunctionReference PassThrough::constructor;

Napi::Object PassThrough::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "PassThrough", {
    InstanceMethod("write", &PassThrough::Write)
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("PassThrough", func);
  return exports;
}

PassThrough::PassThrough(const Napi::CallbackInfo& info) 
: Napi::ObjectWrap<PassThrough>(info)  {
  // NOOP
}

Napi::Value PassThrough::Write(const Napi::CallbackInfo& info) {
  if (info.Length() < 1 ) {
    throw Napi::Error::New(info.Env(), "1 argument expected");
  }
  if (!info[0].IsBuffer()) {
    throw Napi::Error::New(info.Env(), "The parameter must be a buffer");
  }  
  Napi::Buffer<char> buffer = info[0].As<Napi::Buffer<char>>();
  // Convert buffer to a string and pass it back to JS
  // std::string echo(buffer.Data(), buffer.Length());
  // return Napi::String::New(info.Env(), echo);
  return buffer;
}
