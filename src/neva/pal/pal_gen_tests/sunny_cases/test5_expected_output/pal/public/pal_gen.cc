// This file is generated by PAL generator, do not modify.
// To make changes, modify the source file:
// test5.json

// Redefine NOTIMPLEMENTED_POLICY to log once
#define NOTIMPLEMENTED_POLICY 5
#include "base/logging.h"
#include "pal/public/pal.h"

namespace pal {

NativeControlsInterface* Pal::GetNativeControlsInterface() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace pal