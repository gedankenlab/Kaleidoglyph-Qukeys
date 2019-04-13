// -*- c++ -*-

// TODO: decide the file names
#include "qukeys/Qukeys.h"

#include <Arduino.h>

#include <kaleidoglyph/EventHandlerResult.h>
#include <kaleidoglyph/Key.h>
#include <kaleidoglyph/KeyAddr.h>
#include <kaleidoglyph/KeyArray.h>
#include <kaleidoglyph/KeyEvent.h>
#include <kaleidoglyph/KeyState.h>
#include <kaleidoglyph/Plugin.h>
#include <kaleidoglyph/utils.h>


namespace kaleidoglyph {
namespace qukeys {

// Event handler
EventHandlerResult Plugin::onKeyswitchEvent(KeyEvent& event) {
  // If this plugin isn't active:
  if (! plugin_active_) {
    if (isQukeysKey(event.key)) {
      Qukey qukey = getQukey(event.key);
      event.key = qukey.primaryKey();
    }
    return EventHandlerResult::proceed;
  }

  // If the key toggled on
  if (event.state.toggledOn()) {

    // SpaceCadet emulation; only use alternate (non-modifier) keycode if
    // there's no rollover.
    if (key_queue_length_ > 0) {
      if (queue_head_qukey_.isSpaceCadet()) {
        flushKey(false);
      }
    }

    // If the queue is not empty or the pressed key is a qukey, add it to the queue;
    // otherwise continue to the next event handler
    if (key_queue_length_ > 0 || isQukeysKey(event.key)) {
      enqueueKey(event.addr);
      return EventHandlerResult::abort;
    } else {
      return EventHandlerResult::proceed;
    }
    
  } else { // event.state.toggledOff()

    // If a key that was in the queue is released, flush the queue up to that key:
    int8_t queue_index = searchQueue(event.addr);

    // If we didn't find that key in the queue, continue with the next event handler:
    if (queue_index < 0)
      return EventHandlerResult::proceed;

    // flush with alternate keycodes up to (but not including) the released key:
    flushQueue(queue_index);

    // If the released key was a SpaceCadet key, flush it now
    if (queue_head_qukey_.isSpaceCadet()) {
      flushQueue(true);
    }

    // if there's a release delay for that qukey, set the timeout and stop:
    if (key_queue_length_ > 1 && overlap_required_ < 10) {
      uint16_t current_time = Controller::scanStartTime();
      uint16_t overlap_time = current_time - key_queue_[1].start_time;
      qukey_release_delay_ = (overlap_time * 10) / overlap_required_;
      return EventHandlerResult::abort;
    } else if (key_queue_length_ > 0) {
      flushQueue(false);
    }

    // Now correct the event Key value, in case we just flushed a qukey. It
    // needs to be updated based on whatever the Controller has stored in its
    // active keys array.
    event.key = controller_[event.addr];
    return EventHandlerResult::proceed;
  }
}


// Check timeouts and send necessary key events
void Plugin::preKeyswitchScan() {

  // If the queue is empty, there's nothing to do:
  if (key_queue_length_ == 0)
    return;

  // First, we get the current time:
  uint16_t current_time = controller_.scanStartTime();

  // Next, we check the first key in the queue for delayed release
  if (qukey_release_delay_ > 0 && key_queue_length_ > 1) {
    uint16_t elapsed_time = current_time - key_queue_[1].start_time;
    if (elapsed_time > qukey_release_delay_) {
      // prepare release event
      KeyEvent event;
      event.addr  = key_queue_[0].addr;
      event.key   = cKey::clear;
      event.state = cKeyState::release;
      // release qukey primary
      flushQueue(false);
      // send the release event
      controller_.handleKeyEvent(event);
    }
  }

  // Last, we check keys in the queue for hold timeout
  if (key_queue_length_ > 0 &&
      (current_time - key_queue_[0].start_time) > timeout) {
    if (queue_head_qukey_.isSpaceCadet()) {
      // release qukey primary
      flushQueue(false);
    } else {
      // release qukey alternate
      flushQueue(true);
    }
  }

}


// returning a pointer is an experiment here; maybe its better to return the index
inline
const Qukey* Plugin::lookupQukey(Key key) {
  if (QukeysKey::verifyType(key)) {
    byte qukey_index = QukeysKey(key).data();
    if (qukey_index < qukey_count_)
      return &qukeys_[qukey_index];
  }
  return nullptr;
}

// return the Qukey object corresponding to the QukeysKey
inline
Qukey Plugin::getQukey(Key key) const {
  assert(isQukeysKey(key));

  byte qukey_index = QukeysKey(key).data();
  if (qukey_index < qukey_count_)
    return readFromProgmem(qukeys_[qukey_index]);

  // We should never get here, but there's nothing stopping the sketch from
  // having a QukeysKey with index data that's out of bounds.
  return Qukey{};
}

bool Plugin::isQukey(KeyAddr k) const {
  Key key = keymap_[k];
  return isQukeysKey(key);
}

inline
const Qukey* Plugin::lookupQukey(KeyAddr k) {
  return lookupQukey(keymap_[k]);
}

inline
const Qukey* Plugin::lookupQukey(QueueEntry entry) {
  return lookupQukey(entry.addr);
}

// Add a keypress event to the queue while we wait for a qukey's state to be decided
inline
void Plugin::enqueueKey(KeyAddr k) {
  if (key_queue_length_ == queue_max) {
    flushQueue(false); // false => primary key
  }
  key_queue_[key_queue_length_].addr       = k;
  key_queue_[key_queue_length_].start_time = Controller::scanStartTime();
  if (key_queue_length_ == 0) {
    queue_head_qukey_ = getQukey(keymap_[k]);
  }
  ++key_queue_length_;
}

// Search the queue for a given KeyAddr; return index if found, -1 if not
inline
int8_t Plugin::searchQueue(KeyAddr k) {
  for (byte i{0}; i < key_queue_length_; ++i) {
    if (key_queue_[i].addr == k)
      return i;
  }
  return -1; // not found
}

// helper function
inline
QueueEntry Plugin::shiftQueue() {
  QueueEntry entry = key_queue_[0];
  --key_queue_length_;
  for (byte i{0}; i < key_queue_length_; ++i) {
    key_queue_[i] = key_queue_[i + 1]; // check if more efficient to increment here instead of above
  }
  return entry;
}

// flush the first key, with the specified keycode
inline
void Plugin::flushKey(bool use_alternate_key) {
  assert(key_queue_length_ > 0);

  QueueEntry entry = shiftQueue();

  KeyEvent event;
  event.addr  = entry.addr;
  event.state = cKeyState::press;

  Key key = keymap_[entry.addr];
  if (!isQukeysKey(key)) {
    event.key = key;
  } else if (use_alternate_key) {
    event.key = queue_head_qukey_.alternateKey();
  } else {
    event.key = queue_head_qukey_.primaryKey();
  }

  controller_.handleKeyEvent(event);

  // This shouldn't be done unconditionally
  if (key_queue_length_ > 0) {
    KeyAddr k = key_queue_[0].addr;
    queue_head_qukey_ = getQukey(keymap_[k]);
  }
}

// Flush any non-qukeys from the beginning of the queue
inline
void Plugin::flushQueue() {
  while (key_queue_length_ > 0 && isQukeysKey(keymap_[key_queue_[0].addr])) {
    flushKey();
  }
}

inline
void Plugin::flushQueue(bool alt_key) {
  flushKey(alt_key);
  flushQueue();
}

// flush keys up to (but not including) index. qukeys => alternate
inline
void Plugin::flushQueue(int8_t queue_index) {
  while (queue_index > 0) {
    flushKey(true);
    --queue_index;
  }
}

} // namespace qukeys {
} // namespace kaleidoglyph {
