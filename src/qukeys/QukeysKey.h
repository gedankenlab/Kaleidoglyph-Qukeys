// -*- c++ -*-

#pragma once

#include <Arduino.h>

#include <assert.h>
#include "kaleidoglyph/Key.h"
#include "kaleidoglyph/Key/PluginKey.h"

#if defined(KALEIDOGLYPH_QUKEYS_CONSTANTS_H)
#include KALEIDOGLYPH_QUKEYS_CONSTANTS_H
#else
namespace kaleidoglyph {
namespace qukeys {

constexpr byte key_type_id{0b0000000};

}  // namespace qukeys
}  // namespace kaleidoglyph
#endif

namespace kaleidoglyph {
namespace qukeys {

typedef PluginKey<key_type_id> QukeysKey;

constexpr
bool isQukeysKey(Key key) {
  return QukeysKey::verifyType(key);
}

}  // namespace qukeys
}  // namespace kaleidoglyph
