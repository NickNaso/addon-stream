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

#include "stream-writable.h"
#include <iostream>

Napi::FunctionReference StreamWritable::constructor;

Napi::Object StreamWritable::Init(Napi::Env env, Napi::Object exports) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "StreamWritable", {
    InstanceMethod("write", &StreamWritable::Write)
  });

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("StreamWritable", func);
  return exports;
}

StreamWritable::StreamWritable(const Napi::CallbackInfo& info) 
: Napi::ObjectWrap<StreamWritable>(info)  {
  // NOOP
}

void StreamWritable::Write(const Napi::CallbackInfo& info) {
  if (!info[0].IsBuffer()) {
    std::cout << "It's not a buffer" << std::endl;
    // TODO thorw an error
  } else {
    std::cout << "OK" << std::endl;
  }
}

