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
constexpr uint16_t timeout{200};

// For people who are in the habit of simultaneously releasing modifiers with the target
// key (the one to be modified), a release delay can be set. This means that if the qukey
// release is detected first, it won't be flushed from the queue immediately, but will
// wait until this delay expires (in milliseconds) before flushing the qukey in its
// primary state. If, before that happens, the target key's release is detected, it will
// instead get flushed in its alternate state. This is a global default value that will
// apply to all qukeys, but which can be overridden in each Qukey instance. Note that
// setting this value too high (>20ms is not recommended) can result in unintended
// alternate keycodes (i.e. modifiers) during rollover when typing, which is probably
// worse than unintended primary keycodes.
constexpr byte qukey_release_delay{0};

} // namespace qukeys {
} // namespace kaleidoglyph {
