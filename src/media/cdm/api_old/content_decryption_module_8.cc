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

#include "media/cdm/api_old/content_decryption_module_8.h"

namespace cdm {

#if defined(USE_NEVA_MEDIA)
Status ContentDecryptionModule_8::Decrypt(const InputBuffer& encrypted_buffer,
                                          DecryptedBlock* decrypted_buffer) {
  return Decrypt(true, encrypted_buffer, decrypted_buffer);
}

Status ContentDecryptionModule_8::Decrypt(bool use_clear_buffer,
                                          const InputBuffer& encrypted_buffer,
                                          DecryptedBlock* decrypted_buffer) {
  return kSuccess;
}

KeySystem ContentDecryptionModule_8::GetKeySystem() {
  return KeySystem::kNoKeySystem;
}
#endif

}  // namespace cdm
