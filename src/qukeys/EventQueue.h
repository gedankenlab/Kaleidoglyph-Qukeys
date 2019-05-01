// -*- c++ -*-

#pragma once

#include <Arduino.h>

#include <kaleidoglyph/Controller.h>
#include <kaleidoglyph/KeyAddr.h>
#include <kaleidoglyph/KeyEvent.h>
#include <kaleidoglyph/KeyState.h>
#include <kaleidoglyph/utils.h>


namespace kaleidoglyph {

template <byte _max_length,
          typename _Bitfield  = byte,
          typename _Timestamp = uint16_t>
class EventQueue {
  static_assert(_max_length <= (sizeof(_Bitfield) * 8),
                "_Bitfield type too small for _max_length!");
 public:
  byte length() const { return length_; }
  bool isEmpty() const { return (length_ == 0); }
  bool isFull() const { return (length_ == _max_length); }

  void append(KeyEvent event) {
    addrs_[length_]      = event.addr;
    timestamps_[length_] = Controller::scanStartTime();
    bitWrite(release_event_bits_, length_, event.state.toggledOff());
    ++length_;
  }
  void remove(byte index) {
    --length_;
    for (byte i{index}; i < length_; ++i) {
      addrs_[i]      = addrs_[i + 1];
      timestamps_[i] = timestamps_[i + 1];
    }
    static constexpr _Bitfield all = -1;
    _Bitfield tail_mask = all << index;
    _Bitfield head_mask = ~tail_mask;
    _Bitfield tail = (release_event_bits_ >> 1) & tail_mask;
    _Bitfield head = release_event_bits_ & head_mask;
    release_event_bits_ = tail | head;
  }
  void shift() {
    --length_;
    for (byte i{0}; i < length_; ++i) {
      addrs_[i]      = addrs_[i + 1];
      timestamps_[i] = timestamps_[i + 1];
    }
    release_event_bits_ >>= 1;
  }
  void clear() {
    length_ = 0;
    release_event_bits_ = 0;
  }

  KeyAddr addr(byte index) const { return addrs_[index]; }

  _Timestamp timestamp(byte index) const { return timestamps_[index]; }

  bool isRelease(byte index) const {
    return bitRead(release_event_bits_, index);
  }
  bool isPress(byte index) const { return !isRelease(index); }

  KeyEvent head() const {
    KeyEvent event;
    event.addr = addrs_[0];
    event.state =
        bitRead(release_event_bits_, 0) ? cKeyState::release : cKeyState::press;
    return event;
  }

 private:
  byte       length_{0};
  KeyAddr    addrs_[_max_length];
  _Timestamp timestamps_[_max_length];
  _Bitfield  release_event_bits_;
};

}  // namespace kaleidoglyph
