// Copyright (c) 2018 LG Electronics, Inc.
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

#ifndef CONTENT_PUBLIC_COMMON_NEVA_WEB_PREFERENCES_NEVA_H_
#define CONTENT_PUBLIC_COMMON_NEVA_WEB_PREFERENCES_NEVA_H_

namespace content {

struct WebPreferencesNeva {
  WebPreferencesNeva()
      : back_history_api_disabled(false),
        keep_alive_webapp(false),
        gpu_rasterization_allowed(true) {}
  bool back_history_api_disabled;
  bool keep_alive_webapp;
  bool gpu_rasterization_allowed;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_NEVA_WEB_PREFERENCES_NEVA_H_
