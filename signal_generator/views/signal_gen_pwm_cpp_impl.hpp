#pragma once

#include "../menu/menu.hpp"
#include "../menu/menu_items.hpp"
#include "signal_gen_pwm_cpp.h"
#include <memory>

namespace SignalGen {

class PwmView {
public:
  PwmView();
  ~PwmView();

  View *get_view() { return view; }

  void set_callback(SignalGenPwmViewCallback callback, void *context);
  void set_params(uint8_t channel_id, uint32_t freq, uint8_t duty);

private:
  static void draw_callback(Canvas *canvas, void *_model);
  static bool input_callback(InputEvent *event, void *context);

  void setup_menu();
  void on_value_changed(const MenuItem *item);

  View *view;
  Menu *menu;
  SignalGenPwmViewCallback callback;
  void *callback_context;

  // Menu items
  ChoiceMenuItem *channel_item;
  NumberValueMenuItem *freq_item;
  NumberValueMenuItem *duty_item;
};

} // namespace SignalGen