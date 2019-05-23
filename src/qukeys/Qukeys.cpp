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
  // This function is a bit misleading. All it does is add the event to the
  // queue and abort; the processing of the queue now happens in the pre-scan
  // hook instead. I tried including tests here for events that could bypass the
  // event queue, but it increases the size of the binary by more than seems
  // worthwhile (66 bytes).
  event_queue_.append(event);
  // This seemingly-unnecessary call guarantees that the queue can't overflow,
  // even if we get multiple events in a single scan cycle.
  processQueue();
  return EventHandlerResult::abort;
}

// Run once each scan cycle
void Plugin::preKeyswitchScan() {
  // First, flush any events that we can from the queue.
  processQueue();

  // After that, if there's nothing left in the queue, we're done.
  if (event_queue_.isEmpty() || release_delayed_for_tap_hold_) {
    return;
  }

  // Last, check to see if the qukey at the head of the queue has timed out.
  uint16_t current_time = Controller::scanStartTime();
  uint16_t elapsed_time = current_time - event_queue_.timestamp(0);
  if (elapsed_time < hold_timeout) {
    return;
  }

  // The qukey has timed out, so we set the key value depending on which type of
  // qukey it is. For SpaceCadet keys, the modifier is the primary value, but
  // others use the alternate (which isn't necessarily a modifier, but will be
  // in most normal use cases).
  KeyEvent event = event_queue_.head();
  Qukey qukey = getQukey(keymap_[event.addr]);
  event.key = qukey.holdKey();
  event.caller = EventHandlerId::qukeys;
  event_queue_.shift();
  controller_.handleKeyEvent(event);
}

// This function flushes any events that are ready from the queue. In most
// cases, that means the qukey at the head of the queue (if its state can be
// determined), and any subsequent events up to the next qukey. After it runs,
// the head of the queue is guaranteed to be a qukey press whose state is still
// indeterminite.
void Plugin::processQueue() {
  KeyEvent event;
  while (updateFlushEvent(event)) {
    event.caller = EventHandlerId::qukeys;
    controller_.handleKeyEvent(event);
  }
}

// This is the core function. It determines from the queue of events whether or
// not the qukey's state can be determined. If it can, it updates `queued_event`
// accordingly and returns `true`. If we still need to wait on either a timeout
// or another input event (probably a key release), it returns `false`,
// signalling `processQueue()` to stop. It is also possible for non-modifier
// release events without matching press events in the queue to be flushed out
// of order.
bool Plugin::updateFlushEvent(KeyEvent& queued_event) {
  if (event_queue_.isEmpty()) {
    return false;
  }
  queued_event = event_queue_.head();

  // If the first event in the queue is a release, flush it immediately.
  if (queued_event.state.toggledOff()) {
    // Check to see if we're waiting on a potential tap-hold event.
    if (waitingForTapHold()) {
      return false;
    }
    // If not, clear it, and proceed with sending the release event.
    release_delayed_for_tap_hold_ = false;
    event_queue_.shift();
    return true;
  }

  // The first event in the queue is a press, so look up its value in the
  // keymap. If it's not a QukeysKey, we can flush it now.
  queued_event.key = keymap_[queued_event.addr];
  if (!isQukeysKey(queued_event.key)) {
    event_queue_.shift();
    return true;
  }

  // Now the first event in the queue is a QukeysKey press, so we look up its
  // corresponding Qukey, and record whether or not it's a SpaceCadet key for
  // use later.
  Qukey qukey = getQukey(queued_event.key);

  // If Qukeys is turned off, translate the QukeysKey at the head of the queue
  // to its primary value (regardless of whether or not it's a SpaceCadet key),
  // then flush it from the queue.
  if (! plugin_active_) {
    queued_event.key = qukey.primaryKey();
    event_queue_.shift();
    return true;
  }

  // This is the queue index of the next keypress subsequent to the press of the
  // qukey at the head of the queue. If it's zero, that means there haven't been
  // any at the time of the event we're looking at.
  byte next_keypress_index{0};

  // To make the following code slightly more efficient, record whether or not
  // the qukey at the head of the queue is a SpaceCadet key.
  bool qukey_is_spacecadet = qukey.isSpaceCadet();

  // Now we walk down the queue, and determine the state of the qukey, if there
  // have been any events that cause it to become either primary or alternate.
  for (byte i{1}; i < event_queue_.length(); ++i) {
    // First deal with key press events:
    if (event_queue_.isPress(i)) {
      if (qukey_is_spacecadet) {
        queued_event.key = qukey.primaryKey();
        event_queue_.shift();
        return true;
      }
      if (next_keypress_index == 0) {
        next_keypress_index = i;
      }
      continue;
    }
    // event_queue_[i] is a release event

    // If this is a release of the qukey
    if (event_queue_.addr(i) == queued_event.addr) {
      if (next_keypress_index == 0 || overlap_required_ == 0) {
        if (event_queue_.length() == 2) {
          // The only events in the queue are the press and release of this
          // qukey. We need to flush and send the press event, but first, we
          // record that the release should be delayed in case we get a tap-hold
          // sequence.
          release_delayed_for_tap_hold_ = true;
        }
        // there were no keypresses between the qukey press and its release, or
        // there's no release delay overlap configured.
        queued_event.key = qukey_is_spacecadet ? qukey.alternateKey() : qukey.primaryKey();
        event_queue_.shift();
        return true;
      }
      // calculate release delay and check to see if it has timed out
      uint16_t overlap_start = event_queue_.timestamp(next_keypress_index);
      uint16_t overlap_end = event_queue_.timestamp(i);
      if (releaseDelayed(overlap_start, overlap_end)) {
        continue;
      }
      queued_event.key = qukey.primaryKey();
      event_queue_.shift();
      return true; 
    }

    for (byte j{1}; j < i; ++j) {
      if (event_queue_.isPress(j) &&
          event_queue_.addr(j) == event_queue_.addr(i)) {
        // event_queue_[i] is a release of a key that was pressed subsequent
        // to the qukey
        queued_event.key = qukey.alternateKey();
        event_queue_.shift();
        return true;
      }
    }

    // A key was released that is not in the queue. If it's not a modifier key
    // (including layer shifts), we could send the release event out of
    // order. Without doing so, it is possible to get a tapped non-modifier to
    // repeat because of rollover to a qukey that is held. It also reduces the
    // probability of filling up the event queue (and thus prematurely flushing
    // a qukey in its primary state). However, doing so complicates the code,
    // increases PROGMEM usage by 82 bytes, and weakens the guarantee we can
    // make about preserving keyswitch event order.
  }

  // The queue must always have space for the next event to be added, so if it's
  // still full at this point, we need to flush the qukey. The safest thing to
  // do is to use the primary keycode, because someone is probably mashing on
  // the keys.
  if (event_queue_.isFull()) {
    queued_event.key = qukey.primaryKey();
    event_queue_.shift();
    return true;
  }

  // Return empty (invalid) event as signal to wait.
  return false;
}

bool Plugin::releaseDelayed(uint16_t overlap_start, uint16_t overlap_end) const {
  uint16_t overlap_duration = overlap_end - overlap_start;
  uint32_t limit = (overlap_duration * 100) / overlap_required_;
  limit -= overlap_duration;
  uint8_t  release_timeout = (limit < 256) ? limit : 255;
  uint16_t current_time = Controller::scanStartTime();
  uint16_t elapsed_time = current_time - overlap_end;
  return (elapsed_time < release_timeout);
}


// Return false to signal that the release event at the head of the queue should
// proceed, and true if it should be delayed until the state of the potential
// tap-hold composite event has been determined.
bool Plugin::waitingForTapHold() {
  // This must only be called if the event queue has already been determined to
  // be not empty, and the first event is the release of a qukey.
  assert(event_queue_.length > 0);
  assert(event_queue_.isRelease(0));

  // If there's not potential tap-hold waiting for a timeout, proceed.
  if (! release_delayed_for_tap_hold_) {
    return false;
  }

  // These helper variables are here to improve code clarity. It's possible that
  // the binary would be different if I only declared them when necessary; I
  // should check.
  uint16_t current_time = Controller::scanStartTime();
  uint16_t elapsed_time;

  // If there's only one event in the queue (the qukey release event), we only
  // need to check to see if it has timed out.
  if (event_queue_.length() == 1) {
    elapsed_time = current_time - event_queue_.timestamp(0);
    if (elapsed_time > tap_hold_timeout) {
      // The release event has timed out.
      return false;
    }
    // If it hasn't timed out yet, we're done.
    return true;
  }

  // If there are any events in the queue from keys other than the qukey, give
  // up; it gets needlessly complicated if we try to support rollover.
  for (byte i{1}; i < event_queue_.length(); ++i) {
    if (event_queue_.addr(i) != event_queue_.addr(0)) {
      return false;
    }
  }

  // The queue is a series of alternating release and press events, all on the
  // same key, starting with the first release. What we do next only depends on
  // the number of events in the queue.
  if (event_queue_.length() == 2) {
    // Just one release and the second press. Check to see if it has timed out.
    elapsed_time = current_time - event_queue_.timestamp(1);
    if (elapsed_time > tap_hold_timeout) {
      // This is a tap-hold composite event; to turn it into a single press &
      // hold in the output, we just need to remove the first two events from
      // the queue.
      event_queue_.clear();
      release_delayed_for_tap_hold_ = false;
      // There's nothing left in the queue, and we're not technically waiting to
      // decide on a tap-hold, but we need to return true here anyway as a
      // signal that processing of the queue should stop because it's empty.
    }
    return true;
  }

  // If we get here, there are three (or possibly more) events in the queue, so
  // the qukey has been tapped twice.
  return false;
}


// return the Qukey object corresponding to the QukeysKey
Qukey Plugin::getQukey(Key key) const {
  assert(isQukeysKey(key));

  byte qukey_index = QukeysKey(key).data();
  if (qukey_index < qukey_count_)
    return readFromProgmem(qukeys_[qukey_index]);

  // We should never get here, but there's nothing stopping the sketch from
  // having a QukeysKey with index data that's out of bounds.
  return Qukey{};
}

} // namespace qukeys {
} // namespace kaleidoglyph {
