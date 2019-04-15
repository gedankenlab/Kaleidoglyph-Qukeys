// -*- c++ -*-

#include <Arduino.h>

#include <kaleidoglyph/Controller.h>
#include <kaleidoglyph/EventHandlerResult.h>
#include <kaleidoglyph/Key.h>
#include <kaleidoglyph/KeyAddr.h>
#include <kaleidoglyph/Keymap.h>
#include <kaleidoglyph/Plugin.h>
#include <kaleidoglyph/cKey.h>
#include <kaleidoglyph/EventHandlerId.h>

// static_assert(EventHandlerId::qukeys, "Missing definition");

// -------------------------------------------------------------------------------------
// To override the constants in `qukeys/constants.h`, copy that file into your sketch's
// `src/` directory as `qukeys-constants.h`, then add the following line in the sketch's
// `.kaleidoglyph-builder.conf` file:
//
// LOCAL_CFLAGS='-DQUKEYS_CONSTANTS_H=<qukeys-constants.h>'
// ...or (e.g.):
// LOCAL_CFLAGS='-DKALEIDOGLYPH_SKETCH_H=<Model01-Firmware.h>'
//
#if defined(KALEIDOGLYPH_SKETCH_H)
#include KALEIDOGLYPH_SKETCH_H
#endif

#if defined(QUKEYS_CONSTANTS_H)
#include QUKEYS_CONSTANTS_H
#else
#include "qukeys/constants.h"
#endif
// -------------------------------------------------------------------------------------

#include "qukeys/QukeysKey.h"

namespace kaleidoglyph {
namespace qukeys {

// Qukey structure
class Qukey {
 public:
  constexpr
  Qukey() = default;
  constexpr
  Qukey(const Qukey& other)
      : primary_key_(other.primary_key_), alternate_key_(other.alternate_key_) {}
  constexpr
  Qukey(Key primary_key, Key alternate_key)
      : primary_key_(primary_key), alternate_key_(alternate_key) {}

  Key primaryKey() const {
    return primary_key_;
  }
  Key alternateKey() const {
    return alternate_key_;
  }
  bool isSpaceCadet() const {
    // If the primary `Key` value is a modifier, treat this key as a SpaceCadet key
    return isModifierKey(primaryKey());
  }

 private:
  Key primary_key_{cKey::clear};
  Key alternate_key_{cKey::clear};

};

// QueueEntry structure
struct QueueEntry {
  KeyAddr  addr;
  uint16_t start_time;
};


class Plugin : public kaleidoglyph::Plugin {

 public:
  template<byte _qukey_count>
  Plugin(const Qukey (&qukeys)[_qukey_count], Keymap& keymap, Controller& controller)
      : qukeys_(qukeys), qukey_count_(_qukey_count), keymap_(keymap), controller_(controller) {}

  void activate(void) {
    plugin_active_ = true;
  }
  void deactivate(void) {
    plugin_active_ = false;
  }
  void toggle(void) {
    plugin_active_ = !plugin_active_;
  }

  EventHandlerResult onKeyswitchEvent(KeyEvent& event);

  void preKeyswitchScan();

  void setMinimumOverlap(byte percentage) {
    overlap_required_ = percentage;
    if (overlap_required_ >= 100) {
      overlap_required_ = 0;
    }
  }

 private:
  // An array of Qukey objects
  const Qukey* const qukeys_;
  const byte         qukey_count_;

  // A reference to the keymap for lookups
  Keymap& keymap_;

  // A reference to the controller for sending delayed keyswitch events
  Controller& controller_;

  // The queue of keypress events
  QueueEntry key_queue_[queue_max];
  byte       key_queue_length_{0};

  // The qukey at the head of the queue
  Qukey queue_head_qukey_;

  // Release delay for key at head of queue
  byte qukey_release_delay_{0};

  // Percentage overlap required for subsequent key
  byte overlap_required_{90};

  // Runtime controls
  bool plugin_active_{true};

  bool isQukey(KeyAddr k) const;
  Qukey getQukey(Key key) const;

  const Qukey* lookupQukey(Key key);
  const Qukey* lookupQukey(KeyAddr k);
  const Qukey* lookupQukey(QueueEntry entry);

  void enqueueKey(KeyAddr k);
  int8_t searchQueue(KeyAddr k);
  QueueEntry shiftQueue();
  void flushQueue(bool alt_key);
  void flushQueue(int8_t queue_index);
  void flushKey(bool alt_key = false);
  void flushQueue();
  void updateReleaseDelay();
  
};

} // namespace qukeys {
} // namespace kaleidoglyph {
