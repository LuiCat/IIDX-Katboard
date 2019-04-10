#include <Keyboard.h>
#include "Joystick.h"

#define ENABLE_KEYBOARD 0

///////////// Buttons /////////////

const int btn_count = 10;
const int btn_pins[] = {4, 5, 6, 7, 8, 9, 10, SCK, MISO, MOSI};
const int led_pins[] = {A0, A1, A2, A3, A4, A5, 1, 11, 12, 13};
const int keys[] = {'d', '1', 'f', '2', 'j', '3', 'k', KEY_ESC, KEY_RETURN, KEY_BACKSPACE};

int states_raw[btn_count] = {};
int states[btn_count] = {};

unsigned long last_time[btn_count] = {}; 
const unsigned long min_hold_time = 33000; // us
const unsigned long min_release_time = 50000; // us

void set_button(int i, int state) {
  states[i] = state;
  if (state) {
#if ENABLE_KEYBOARD
    Keyboard.press(keys[i]);
#endif
    Joystick.buttons |= (uint16_t)1 << i;
    digitalWrite(led_pins[i], LOW);
  } else {
#if ENABLE_KEYBOARD
    Keyboard.release(keys[i]);
#endif
    Joystick.buttons &= ~((uint16_t)1 << i);
    digitalWrite(led_pins[i], HIGH);
  }
}

///////////// Turntable ///////////

const float min_speed = 10; // pulse/s
const unsigned long max_interval = 80000; // us
const float damping_axis = 0.90f;
const int sensivity_arrow = 20; // pulse/keypress

const int pins_tt[] = {2, 3};
const int keys_tt[] = {KEY_DOWN_ARROW, KEY_UP_ARROW};
const int key_hold_tt = '`';
int pos_tt = 0;
int state_tt = 0;

void interrupt_tt_a() {
  int pin_a = digitalRead(pins_tt[0]);
  int pin_b = digitalRead(pins_tt[1]);
  if (pin_a ^ pin_b) {
    ++pos_tt;
  } else {
    --pos_tt;
  }
}

void set_button_tt(int state) {
  if (state_tt != 0) {
#if ENABLE_KEYBOARD
    Keyboard.release(key_hold_tt);
#endif
    Joystick.buttons &= ~((uint16_t)1 << 10);
  }
  if (state != 0) {
#if ENABLE_KEYBOARD
    Keyboard.press(key_hold_tt);
#endif
    Joystick.buttons |= (uint16_t)1 << 10;
  }
  state_tt = state;
}

void set_axis_tt() {
  static int last_pos_tt = 0;
  static int last_pos_arrow_tt = 0;
  
  int curr_pos_tt = pos_tt;

  last_pos_tt = curr_pos_tt;
  Joystick.xAxis = (curr_pos_tt >> 2);

  int diff_pos_arrow_tt = curr_pos_tt - last_pos_arrow_tt;
  if (diff_pos_arrow_tt >= sensivity_arrow) {
#if ENABLE_KEYBOARD
    Keyboard.write(keys_tt[0]);
#endif
    last_pos_arrow_tt += sensivity_arrow;
  } else if (diff_pos_arrow_tt <= -sensivity_arrow) {
#if ENABLE_KEYBOARD
    Keyboard.write(keys_tt[1]);
#endif
    last_pos_arrow_tt -= sensivity_arrow;
  }
}

///////////// Program /////////////

void setup() {
  for(int i = 0; i < btn_count; ++i) {
    pinMode(btn_pins[i], INPUT_PULLUP);
  }
  for(int i = 0; i < btn_count; ++i) {
    digitalWrite(led_pins[i], HIGH);
    pinMode(led_pins[i], OUTPUT);
  }
  
  pinMode(pins_tt[0], INPUT_PULLUP);
  pinMode(pins_tt[1], INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pins_tt[0]), interrupt_tt_a, CHANGE);
  
#if ENABLE_KEYBOARD
  Keyboard.begin();
#endif
}

void loop() {
  delay(1);
  
  unsigned long curr_time = micros();
  int state;
  
  for (int i = 0; i < btn_count; ++i) {
    state = (digitalRead(btn_pins[i]) == LOW);
    if (states[i] != state && curr_time - last_time[i] > (states[i] ? min_hold_time : min_release_time)) {
      last_time[i] = curr_time;
      set_button(i, state);
    } else if (states[i] == state && states_raw[i] != state) {
      last_time[i] = curr_time;
    }
    states_raw[i] = state;
  }

  static int last_pos_tt = pos_tt;
  static unsigned long last_time_tt = micros();

  int curr_pos_tt = pos_tt;
  unsigned long time_tt = curr_time;
  
  state = state_tt;
  if (last_pos_tt != curr_pos_tt) {
    state = 0;
    if (abs(curr_pos_tt - last_pos_tt) > (time_tt - last_time_tt) * 1e-6f * min_speed) {
      state = (curr_pos_tt - last_pos_tt > 0 ? 1 : -1);
    }
    last_pos_tt = curr_pos_tt;
    last_time_tt = time_tt;
  } else if (state_tt != 0 && (time_tt - last_time_tt) > max_interval) {
    state = 0;
  }
  if (state_tt != state) {
    set_button_tt(state);
  }
  
  set_axis_tt();
  
  Joystick.sendState();
}
