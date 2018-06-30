// -*- c++ -*-

// This is a default Qukeys configuration file, which should be included in the user's
// sketch by the pre-build script. It may be necessary to break it up into different
// sections, because each plugin's ultimate configuration may depend on other plugin
// definitions. In particular, the include directives should probably all be put at the
// top of the file. For example, a Qukey definition might need to refer to some other
// plugin's Key variant.

#include <Kaleidoglyph-Qukeys.h>

namespace kaleidoglyph {
namespace qukeys {

// Ultimately, I want these to be created automatically from the keymap definition (not
// the one in code, but a special keymap config file)

// const PROGMEM
Qukey qukey_defs[] = {
  {Key_F, Key_LeftShift},    // 0
  {Key_D, Key_LeftControl},  // 1
  {Key_S, Key_LeftAlt},      // 2
  {Key_A, Key_LeftGui},      // 3
};

constexpr byte qukey_count = sizeof(qukey_defs)/sizeof(qukey_defs[0]);

enum class QukeyIndex : byte {
  qk_f_lsft,
  qk_d_lctl,
  qk_s_lalt,
  qk_a_lgui,
  count,
};

const Qukey qukey_defs[QukeyIndex::count] = {
  
};

constexpr Qukey qk_f_lsft {Key_F, Key_LeftShift};
constexpr Qukey qk_d_lctl {Key_D, Key_LeftControl};
constexpr Qukey qk_s_lalt {Key_S, Key_LeftAlt};
constexpr Qukey qk_a_lgui {Key_A, Key_LeftGui};

const PROGMEM
Qukey qukey_defs[] = {
  qk_f_lsft,
  qk_d_lctl,
  qk_s_lalt,
  qk_a_lgui,
};

constexpr QukeysKey qkk_f_lsft {0};
constexpr QukeysKey qkk_d_lctl {1};
constexpr QukeysKey qkk_s_lalt {2};
constexpr QukeysKey qkk_a_lgui {3};

}
}


class QukeyArray {
 private:
  
};
