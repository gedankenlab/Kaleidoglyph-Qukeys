// -*- c++ -*-

#pragma once

namespace kaleidoglyph {
namespace qukeys {

// Maximum size of the key queue; if one more key is pressed when the queue is full, the
// first key in the queue (and any subsequent non-qukeys) will be flushed in its primary
// state. 8 should be more than enough for any normal typing situation, and qukeys is best
// turned off completely for gaming.
constexpr byte queue_max{8};

// If a qukey is in the queue at least this long (in milliseconds), it will be flushed
// from the queue in its alternate state. This allows qukey modifiers to be used with
// external pointing devices. This timeout value does not affect how long it takes for a
// qukey to be flushed when typing normally, because a key release will cause it to be
// flushed before the timeout expires.
constexpr uint16_t hold_timeout{200};

// To enable the tap-hold feature, we need another timeout value. When a qukey
// is tapped, it's release event will be delayed by this amount. If it is tapped
// again before that timeout, the release event will again be delayed by the
// same amount (after the second press). If the qukey hasn't been released by
// the second timeout, the first release event and the second press event will
// be ignored, and the keyboard will behave as if the key was simply pressed and
// held since the initial press event.
constexpr byte tap_hold_timeout{200};

} // namespace qukeys {
} // namespace kaleidoglyph {
