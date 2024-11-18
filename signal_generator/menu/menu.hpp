#pragma once

#include "menu_item.hpp"
#include <memory>

namespace SignalGen {

class Menu {
public:
  static constexpr size_t MAX_ITEMS = 16; // Maximum number of menu items

  Menu() : items_count_(0), selected_index_(-1) {}
  ~Menu() = default;

  // Add a menu item
  bool add_item(MenuItem *item) {
    if (items_count_ >= MAX_ITEMS) {
      return false;
    }
    items_[items_count_++] = item;
    if (selected_index_ == -1) {
      selected_index_ = 0;
    }
    return true;
  }

  // Draw the menu
  void draw(Canvas *canvas) {
    uint8_t y = 0;
    const uint8_t item_height = 16; // Can be adjusted based on UI needs

    for (size_t i = 0; i < items_count_; i++) {
      items_[i]->draw(canvas, y, (int)i == selected_index_);
      y += item_height;
    }
  }

  // Handle input events
  bool handle_input(InputEvent *event) {
    if (items_count_ == 0)
      return false;

    // First let the selected item handle the input
    if (items_[selected_index_]->handle_input(event)) {
      return true;
    }

    // If not handled by item, handle menu navigation
    if (event->type == InputTypeShort || event->type == InputTypeRepeat) {
      switch (event->key) {
      case InputKeyUp:
        if (selected_index_ > 0) {
          items_[selected_index_]->on_exit();
          selected_index_--;
          items_[selected_index_]->on_enter();
          return true;
        }
        break;

      case InputKeyDown:
        if (selected_index_ < (int)(items_count_ - 1)) {
          items_[selected_index_]->on_exit();
          selected_index_++;
          items_[selected_index_]->on_enter();
          return true;
        }
        break;

      default:
        break;
      }
    }

    return false;
  }

  // Get currently selected item
  MenuItem *get_selected_item() {
    if (selected_index_ >= 0 && selected_index_ < (int)items_count_) {
      return items_[selected_index_];
    }
    return nullptr;
  }

private:
  MenuItem *items_[MAX_ITEMS];
  size_t items_count_;
  int selected_index_;
};

} // namespace SignalGen
