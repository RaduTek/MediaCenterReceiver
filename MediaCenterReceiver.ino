#include "MCE_Remote.h"
#include "Logitech_Z_Cinema_Remote.h"

#define DECODE_NEC
#define DECODE_RC6

#include <IRremote.hpp>
#include "HID-Project.h"

// #define DEBUG
#define REPEAT_INTERVAL 110

// Wheel mode constants
#define WHEEL_MODE_VOLUME           0
#define WHEEL_MODE_SCROLL           1
#define WHEEL_MODE_ARROW_HORIZONTAL 2  // Left/Right arrows
#define WHEEL_MODE_ARROW_VERTICAL   3  // Up/Down arrows

// Runtime variables, used to handle current remote state
uint16_t      key_code        = 0;
bool          key_held        = false;
bool          hold_confirmed  = false;
unsigned long last_activity   = 0;
uint8_t       wheel_phase     = 0;
unsigned long press_time      = 0;
uint8_t       wheel_mode      = WHEEL_MODE_VOLUME;

// Keypad state variables
uint8_t       last_keypad_key    = 0xFF;
uint8_t       keypad_char_index  = 0;
unsigned long last_keypad_time   = 0;
bool          keypad_alpha_mode  = true;
bool          keypad_shift       = false;

const char keypad_map[][10] PROGMEM = {
  {' ', 0, 0, 0, 0, 0, 0, 0, 0, 0},              // 0: space
  {'.', ',', '?', '!', ':', ';', '-', '(', ')', 0}, // 1: punctuation
  {'a', 'b', 'c', '2', 0, 0, 0, 0, 0, 0},        // 2: abc
  {'d', 'e', 'f', '3', 0, 0, 0, 0, 0, 0},        // 3: def
  {'g', 'h', 'i', '4', 0, 0, 0, 0, 0, 0},        // 4: ghi
  {'j', 'k', 'l', '5', 0, 0, 0, 0, 0, 0},        // 5: jkl
  {'m', 'n', 'o', '6', 0, 0, 0, 0, 0, 0},        // 6: mno
  {'p', 'q', 'r', 's', '7', 0, 0, 0, 0, 0},      // 7: pqrs
  {'t', 'u', 'v', '8', 0, 0, 0, 0, 0, 0},        // 8: tuv
  {'w', 'x', 'y', 'z', '9', 0, 0, 0, 0, 0}       // 9: wxyz
};

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  IrReceiver.begin(2);
  Consumer.begin();
  BootKeyboard.begin();
  System.begin();
  Mouse.begin();
}

void loop() {
  if (IrReceiver.decode()) {

    bool is_repeat =
      IrReceiver.decodedIRData.flags &
      (IRDATA_FLAGS_IS_AUTO_REPEAT | IRDATA_FLAGS_IS_REPEAT);

    // Calculate current key code from IR data
    uint16_t current_key_code = 0;
    if (IrReceiver.decodedIRData.protocol == RC6 &&
        IrReceiver.decodedIRData.address == MCE_ADDRESS) {
      current_key_code = REMOTE_TYPE_MCE << 8 | IrReceiver.decodedIRData.command;
    } else if (IrReceiver.decodedIRData.protocol == NEC &&
               IrReceiver.decodedIRData.address == LZC_ADDRESS) {
      current_key_code = REMOTE_TYPE_LZC << 8 | IrReceiver.decodedIRData.command;
    }

    bool key_changed = (current_key_code != 0 && current_key_code != key_code);

    if (!is_repeat || key_changed) {
      // New IR frame or different key

      releaseHeldKey();

      hold_confirmed = false;
      wheel_phase = 0;

      if (current_key_code != 0) {
        key_code = current_key_code;
        key_held = decode_key(key_code, false);
      }
      
      press_time = millis();
      last_activity = press_time;

#ifdef DEBUG
      Serial.print(F("Key "));
      Serial.println(key_code, HEX);
#endif

    } else {
      // Repeat frame (same key)
      
      last_activity = millis();

      if (key_held) {
        hold_confirmed = true;
      } else {
        decode_key(key_code, true);
      }
    }

    IrReceiver.resume();
  }

  // Release logic
  if (key_held) {
    if (hold_confirmed) {
      if (millis() - last_activity >= REPEAT_INTERVAL) {
        releaseHeldKey();
      }
    }
    else {
      if (millis() - press_time >= REPEAT_INTERVAL) {
        releaseHeldKey();
      }
    }
  }

}

void releaseHeldKey() {
  Consumer.releaseAll();
  Keyboard.releaseAll();
  System.releaseAll();
  key_held = false;
  hold_confirmed = false;
}

void handle_keypad(uint8_t key) {
  unsigned long current_time = millis();
  
  // Handle star (shift)
  if (key == 10) {
    keypad_shift = !keypad_shift;
#ifdef DEBUG
    Serial.print(F("Shift: "));
    Serial.println(keypad_shift ? "ON" : "OFF");
#endif
    return;
  }
  
  // Handle pound (mode switch)
  if (key == 11) {
    keypad_alpha_mode = !keypad_alpha_mode;
#ifdef DEBUG
    Serial.print(F("Mode: "));
    Serial.println(keypad_alpha_mode ? "ALPHA" : "NUMERIC");
#endif
    return;
  }

  if (key == 0 && keypad_alpha_mode) {
    Keyboard.write(KEY_SPACE);
    return;
  }
  
  // Handle numeric keys 0-9
  if (key > 9) return;
  
  // In numeric mode, just send the digit
  if (!keypad_alpha_mode) {
    Keyboard.write('0' + key);
#ifdef DEBUG
    Serial.print(F("Numeric: "));
    Serial.println(key);
#endif
    return;
  }
  
  // Alpha mode T9 logic
  bool same_key = (key == last_keypad_key);
  bool within_timeout = (current_time - last_keypad_time) < 1000;
  
  if (same_key && within_timeout) {
    // Same key pressed within timeout - cycle to next character
    Keyboard.write(KEY_BACKSPACE);
    keypad_char_index++;
  } else {
    // Different key or timeout - reset to first character
    keypad_char_index = 0;
  }
  
  // Get character from map
  char ch = 0;
  uint8_t idx = 0;
  while (idx < 10) {
    ch = pgm_read_byte(&keypad_map[key][idx]);
    if (ch == 0) {
      // Wrap around to first character
      keypad_char_index = 0;
      ch = pgm_read_byte(&keypad_map[key][0]);
      break;
    }
    if (idx == keypad_char_index) {
      break;
    }
    idx++;
  }
  
  // Apply shift (capitalize)
  if (keypad_shift && ch >= 'a' && ch <= 'z') {
    ch = ch - 'a' + 'A';
  }
  
  // Send character
  Keyboard.write(ch);
  
#ifdef DEBUG
  Serial.print(F("Keypad: "));
  Serial.print((char)ch);
  Serial.print(F(" (index "));
  Serial.print(keypad_char_index);
  Serial.println(F(")"));
#endif
  
  last_keypad_key = key;
  last_keypad_time = current_time;
}

bool decode_key(uint16_t key, bool is_repeat) {
  switch (key) {
    case MCE_KEY_POWER:
    case LZC_KEY_POWER:
      System.press(HID_SYSTEM_SLEEP);
      return true;

    // Navigation
    case MCE_KEY_UP:
    case LZC_KEY_UP:
      Keyboard.press(KEY_UP_ARROW);
      return true;

    case MCE_KEY_DOWN:
    case LZC_KEY_DOWN:
      Keyboard.press(KEY_DOWN_ARROW);
      return true;

    case MCE_KEY_LEFT:
    case LZC_KEY_LEFT:
      Keyboard.press(KEY_LEFT_ARROW);
      return true;

    case MCE_KEY_RIGHT:
    case LZC_KEY_RIGHT:
      Keyboard.press(KEY_RIGHT_ARROW);
      return true;
      
    case MCE_KEY_OK:
    case LZC_KEY_OK:
    case LZC_KEY_WHEEL_CLICK:
      Keyboard.press(KEY_ENTER);
      return true;

    case MCE_KEY_BACK:
    case LZC_KEY_BACK:
      Consumer.press(CONSUMER_BROWSER_BACK);
      return true;

    case MCE_KEY_MORE:
    case LZC_KEY_MORE:
      Keyboard.press(KEY_MENU);
      return true;

    case MCE_KEY_CHANNEL_UP:
    case LZC_KEY_CHANNEL_UP:
      Keyboard.press(KEY_PAGE_UP);
      return true;

    case MCE_KEY_CHANNEL_DOWN:
    case LZC_KEY_CHANNEL_DOWN:
      Keyboard.press(KEY_PAGE_DOWN);
      return true;

    // Media Control
    case MCE_KEY_RECORD:
    case LZC_KEY_RECORD:
      Consumer.press(HID_CONSUMER_RECORD);
      return true;

    case MCE_KEY_PLAY:
    case LZC_KEY_PLAY:
      Consumer.press(HID_CONSUMER_PLAY);
      return true;

    case MCE_KEY_PAUSE:
    case LZC_KEY_PAUSE:
      Consumer.press(HID_CONSUMER_PAUSE);
      return true;

    case MCE_KEY_STOP:
    case LZC_KEY_STOP:
      Consumer.press(HID_CONSUMER_STOP);
      return true;

    case MCE_KEY_PREVIOUS:
    case LZC_KEY_PREVIOUS:
      Consumer.press(HID_CONSUMER_SCAN_PREVIOUS_TRACK);
      return true;

    case MCE_KEY_NEXT:
    case LZC_KEY_NEXT:
      Consumer.press(HID_CONSUMER_SCAN_NEXT_TRACK);
      return true;

    case MCE_KEY_FASTFORWARD:
    case LZC_KEY_FASTFORWARD:
      Consumer.press(HID_CONSUMER_FAST_FORWARD);
      return true;

    case MCE_KEY_REWIND:
    case LZC_KEY_REWIND:
      Consumer.press(HID_CONSUMER_REWIND);
      return true;
    
    case MCE_KEY_EJECT:
      Consumer.press(HID_CONSUMER_EJECT);
      return true;

    // Volume Keys
    case MCE_KEY_MUTE:
    case LZC_KEY_MUTE:
      Consumer.press(HID_CONSUMER_MUTE);
      return true;

    case MCE_KEY_VOL_UP:
      Consumer.press(HID_CONSUMER_VOLUME_INCREMENT);
      return true;

    case MCE_KEY_VOL_DOWN:
      Consumer.press(HID_CONSUMER_VOLUME_DECREMENT);
      return true;

    // Volume Wheel / Scroll Wheel / Arrow Keys
    case LZC_KEY_WHEEL_CW:
      if (is_repeat && (++wheel_phase & 1)) return false;
      if (wheel_mode == WHEEL_MODE_VOLUME) {
        Consumer.write(HID_CONSUMER_VOLUME_INCREMENT);
      } else if (wheel_mode == WHEEL_MODE_SCROLL) {
        Mouse.move(0, 0, -1);  // Scroll up
      } else if (wheel_mode == WHEEL_MODE_ARROW_HORIZONTAL) {
        Keyboard.write(KEY_RIGHT_ARROW);
      } else if (wheel_mode == WHEEL_MODE_ARROW_VERTICAL) {
        Keyboard.write(KEY_DOWN_ARROW);
      }
      return false;
    
    case LZC_KEY_WHEEL_CCW:
      if (is_repeat && (++wheel_phase & 1)) return false;
      if (wheel_mode == WHEEL_MODE_VOLUME) {
        Consumer.write(HID_CONSUMER_VOLUME_DECREMENT);
      } else if (wheel_mode == WHEEL_MODE_SCROLL) {
        Mouse.move(0, 0, 1);  // Scroll down
      } else if (wheel_mode == WHEEL_MODE_ARROW_HORIZONTAL) {
        Keyboard.write(KEY_LEFT_ARROW);
      } else if (wheel_mode == WHEEL_MODE_ARROW_VERTICAL) {
        Keyboard.write(KEY_UP_ARROW);
      }
      return false;

    // Special
    case MCE_KEY_MEDIA:
    case LZC_KEY_MEDIA:
      Keyboard.press(KEY_LEFT_WINDOWS);
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_ENTER);
      return true;
    
    case MCE_KEY_ASPECT:
    case LZC_KEY_FULLSCREEN:
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_ENTER);
      return true;
    
    case MCE_KEY_DVD_MENU:
    case LZC_KEY_DVD_MENU:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_LEFT_SHIFT);
      Keyboard.press(KEY_M);
      return true;
    
    case MCE_KEY_LIVE_TV:
    case LZC_KEY_LIVE_TV:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_T);
      return true;
     
    case MCE_KEY_GUIDE:
    case LZC_KEY_GUIDE:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_G);
      return true;
    
    case MCE_KEY_RECORDED_TV:
    case LZC_KEY_RECORDED_TV:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_O);
      return true;
    
    case LZC_KEY_MUSIC:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_M);
      return true;
    
    case LZC_KEY_VIDEO:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_E);
      return true;

    case LZC_KEY_PHOTOS:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_I);
      return true;
    
    case MCE_KEY_PRINT:
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_P);
      return true;
    
    // Wheel Mode Switching
    case LZC_KEY_AUDIO_SRS:
      wheel_mode = WHEEL_MODE_SCROLL;
#ifdef DEBUG
      Serial.println(F("Wheel mode: SCROLL"));
#endif
      return true;
    
    case LZC_KEY_AUDIO_HEADPHONE:
      wheel_mode = WHEEL_MODE_VOLUME;
#ifdef DEBUG
      Serial.println(F("Wheel mode: VOLUME"));
#endif
      return true;
    
    case LZC_KEY_AUDIO_MENU:
      wheel_mode = WHEEL_MODE_ARROW_HORIZONTAL;
#ifdef DEBUG
      Serial.println(F("Wheel mode: ARROW LEFT/RIGHT"));
#endif
      return true;
    
    case LZC_KEY_AUDIO_RESET:
      wheel_mode = WHEEL_MODE_ARROW_VERTICAL;
#ifdef DEBUG
      Serial.println(F("Wheel mode: ARROW UP/DOWN"));
#endif
      return true;

    // Numeric Keypad
    case MCE_KEY_NUM_0:
    case LZC_KEY_NUM_0:
      handle_keypad(0);
      return true;
      
    case MCE_KEY_NUM_1:
    case LZC_KEY_NUM_1:
      handle_keypad(1);
      return true;
      
    case MCE_KEY_NUM_2:
    case LZC_KEY_NUM_2:
      handle_keypad(2);
      return true;
      
    case MCE_KEY_NUM_3:
    case LZC_KEY_NUM_3:
      handle_keypad(3);
      return true;
      
    case MCE_KEY_NUM_4:
    case LZC_KEY_NUM_4:
      handle_keypad(4);
      return true;
      
    case MCE_KEY_NUM_5:
    case LZC_KEY_NUM_5:
      handle_keypad(5);
      return true;
      
    case MCE_KEY_NUM_6:
    case LZC_KEY_NUM_6:
      handle_keypad(6);
      return true;
      
    case MCE_KEY_NUM_7:
    case LZC_KEY_NUM_7:
      handle_keypad(7);
      return true;
      
    case MCE_KEY_NUM_8:
    case LZC_KEY_NUM_8:
      handle_keypad(8);
      return true;
      
    case MCE_KEY_NUM_9:
    case LZC_KEY_NUM_9:
      handle_keypad(9);
      return true;
      
    case MCE_KEY_NUM_STAR:
    case LZC_KEY_NUM_STAR:
      handle_keypad(10);
      return true;
      
    case MCE_KEY_NUM_POUND:
    case LZC_KEY_NUM_POUND:
      handle_keypad(11);
      return true;

    case MCE_KEY_CLEAR:
    case LZC_KEY_CLEAR:
      Keyboard.press(KEY_BACKSPACE);
      return true;
    
    case MCE_KEY_ENTER:
    case LZC_KEY_ENTER:
      Keyboard.press(KEY_ENTER);
      return true;

    default:
      return false;
  }
}
