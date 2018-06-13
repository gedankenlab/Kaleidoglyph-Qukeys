// -*- c++ -*-

#include <Arduino.h>

#include KALEIDOGLYPH_HARDWARE_H
#include KALEIDOGLYPH_KEYADDR_H
#include <kaleidoglyph/Key.h>
#include <kaleidoglyph/Plugin.h>
#include <kaleidoglyph/Keymap.h>
#include <kaleidoglyph/Controller.h>
#include <kaleidoglyph/cKey.h>

#include "qukeys/QukeysKey.h"

namespace kaleidoglyph {
namespace qukeys {

// Constants (can be overridden in the sketch)
__attribute__((weak)) extern
constexpr uint16_t timeout{200};

__attribute__((weak)) extern
constexpr byte qukey_release_delay{0};

__attribute__((weak)) extern
constexpr uint16_t grace_period_offset{4096};

__attribute__((weak)) extern
constexpr byte queue_max{8};

// Qukey structure
struct Qukey {
  Key  primary;
  Key  alternate;
  byte release_delay{qukey_release_delay};

  Qukey(Key pri, Key alt, byte delay = qukey_release_delay) : primary(pri),
                                                              alternate(alt),
                                                              release_delay(delay) {}
};

// QueueEntry structure
struct QueueEntry {
  KeyAddr  addr;
  uint16_t start_time;
};


class Plugin : public kaleidoglyph::Plugin {

 public:
  Plugin(const Qukey* const qukeys, const byte qukey_count, Keymap& keymap, Controller& controller)
      : qukeys_(qukeys), qukey_count_(qukey_count), keymap_(keymap), controller_(controller) {}

  void activate(void) {
    plugin_active_ = true;
  }
  void deactivate(void) {
    plugin_active_ = false;
  }
  void toggle(void) {
    plugin_active_ = !plugin_active_;
  }

  bool keyswitchEventHook(KeyEvent& event,
                          kaleidoglyph::Plugin*& caller) override;

  void preScanHook(uint16_t current_time) override;

 private:
  // An array of Qukey objects
  const Qukey* const qukeys_;
  const byte         qukey_count_;

  // A reference to the keymap for lookups
  Keymap& keymap_;

  // A reference to the keymap for lookups
  Controller& controller_;

  // A reference to the (shared) array of active key values
  //KeyArray& active_keys_;

  // The queue of keypress events
  QueueEntry key_queue_[queue_max];
  byte       key_queue_length_{0};

  // Pointer to the qukey at the head of the queue
  const Qukey* queue_head_p_{nullptr};

  // Release delay for key at head of queue
  byte qukey_release_delay_{0};

  // Runtime controls
  bool plugin_active_{true};

  const Qukey* lookupQukey(Key key);
  const Qukey* lookupQukey(KeyAddr k);
  const Qukey* lookupQukey(QueueEntry entry);

  void enqueueKey(KeyAddr k, const Qukey* qp = nullptr);
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
