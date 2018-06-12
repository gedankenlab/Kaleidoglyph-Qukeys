// -*- c++ -*-

// TODO: decide the file names
#include "qukeys/Qukeys.h"

#include <Arduino.h>

#include KALEIDOGLYPH_HARDWARE_H
#include KALEIDOGLYPH_KEYADDR_H
#include <kaleidoglyph/Key.h>
#include <kaleidoglyph/Plugin.h>
#include <kaleidoglyph/KeyState.h>
#include <kaleidoglyph/KeyArray.h>
#include <kaleidoglyph/KeyEvent.h>


namespace kaleidoglyph {
namespace qukeys {


// Event handler
bool Plugin::keyswitchEventHook(KeyEvent& event,
                                kaleidoglyph::Plugin*& caller) {
  // If this plugin isn't active:
  if (! plugin_active_) {
    if (const Qukey* qp = lookupQukey(event.key))
      event.key = qp->primary;
    return true;
  }

  // If Qukeys has already processed this event:
  if (checkCaller(caller))
    return true;

  // If the key toggled on
  if (event.state.toggledOn()) {

    // If the queue is not empty or the pressed key is a qukey, add it to the queue;
    // otherwise continue to the next event handler
    const Qukey* qp = lookupQukey(event.key);
    if (key_queue_length_ > 0 || qp != nullptr) {
      enqueueKey(event.addr, qp);
      return false;
    } else {
      return true;
    }
    
  } else { // event.state.toggledOff()

    // If a key that was in the queue is released, flush the queue up to that key:
    int8_t queue_index = searchQueue(event.addr);

    // If we didn't find that key in the queue, continue with the next event handler:
    if (queue_index < 0)
      return true;

    // flush with alternate keycodes up to (but not including) the released key:
    flushQueue(queue_index);

    // DANGER! here's a spot where we could have a non-empty queue with non-qukey at the
    // head. Maybe I can fix this with more logic in flushQueue(). For the moment, I'm
    // just assuming flushQueue sets qukey_release_delay_ appropriately.

    // if there's a release delay for that qukey, set the timeout and stop:
    if (qukey_release_delay_ > 0) {
      key_queue_[0].start_time = millis() + grace_period_offset;
      return false;
    } else {
      flushQueue(false);
      // send release event for the just-released key
      event.state = cKeyState::released;
      controller_.handleKeyEvent(event, this);
      return true;
    }
  }
}


// Check timeouts and send necessary key events
void Plugin::preScanHook(uint16_t current_time) {

  // If the queue is empty, there's nothing to do:
  if (key_queue_length_ == 0)
    return;

  // First, we get the current time:
  //uint16_t current_time = millis();

  // Next, we check the first key in the queue for delayed release
  int16_t diff_time = key_queue_[0].start_time - current_time;
  if (diff_time > 0 && diff_time < int16_t(grace_period_offset - qukey_release_delay_)) {
    KeyEvent event;
    event.addr  = key_queue_[0].addr;
    event.key   = cKey::clear;
    event.state = cKeyState::released;
    flushQueue(false);
    controller_.handleKeyEvent(event, this); // send the release event
  }

  // Last, we check keys in the queue for hold timeout
  while (key_queue_length_ > 0 &&
         (current_time - key_queue_[0].start_time) > timeout) {
    flushQueue(true);
  }

}


// returning a pointer is an experiment here; maybe its better to return the index
inline
const Qukey* Plugin::lookupQukey(Key key) {
  if (QukeysKey::verify(key)) {
    byte qukey_index = QukeysKey(key).index();
    if (qukey_index < qukey_count_)
      return &qukeys_[qukey_index];
  }
  return nullptr;
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
void Plugin::enqueueKey(KeyAddr k, const Qukey* qp) {
  if (key_queue_length_ == queue_max) {
    flushQueue(false); // false => primary key
  }
  QueueEntry& queue_tail = key_queue_[key_queue_length_];
  queue_tail.addr       = k;
  queue_tail.start_time = millis();
  if (key_queue_length_ == 0) {
    queue_head_p_        = qp;
    qukey_release_delay_ = qp->release_delay;
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
void Plugin::flushKey(bool alt_key) {
  assert(key_queue_length_ > 0);
  QueueEntry entry = shiftQueue();
  // WARNING: queue_head_p_ doesn't reflect key_queue_[0] here
  KeyEvent event;
  if (queue_head_p_ == nullptr) {
    event.key = keymap_[entry.addr];
  } else if (alt_key) {
    event.key = queue_head_p_->alternate;
  } else {
    event.key = queue_head_p_->primary;
  }
  event.addr  = entry.addr;
  event.state = cKeyState::pressed;
  controller_.handleKeyEvent(event, this);
  //shiftQueue();
  queue_head_p_ = lookupQukey(key_queue_[0]);
  // WARNING: qukey_release_delay_ out of sync
  delay(1);
}

inline
void Plugin::updateReleaseDelay() {
  qukey_release_delay_ = queue_head_p_ ? queue_head_p_->release_delay : 0;
}

// Flush any non-qukeys from the beginning of the queue
inline
void Plugin::flushQueue() {
  while (key_queue_length_ > 0 && queue_head_p_ == nullptr) {
    flushKey();
  }
  updateReleaseDelay();
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
  updateReleaseDelay();
}

} // namespace qukeys {
} // namespace kaleidoglyph {