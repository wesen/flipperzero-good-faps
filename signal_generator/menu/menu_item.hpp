#pragma once

#include "input/input.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <gui/canvas.h>

namespace SignalGen {

// Forward declaration
class Menu;

/**
 * @brief Abstract base class for menu items
 */
class MenuItem {
public:
    virtual ~MenuItem() = default;

    // Core interface methods
    virtual void draw(Canvas* canvas, uint8_t y, bool selected) = 0;
    virtual bool handle_input(InputEvent* event) = 0;
    virtual void on_enter() = 0;
    virtual void on_exit() = 0;

    // Common methods
    virtual const char* get_name() const = 0;
    virtual const char* get_value() const = 0;

    // Optional callback when value changes
    using ValueChangedCallback = std::function<void(const MenuItem*)>;
    void set_value_changed_callback(ValueChangedCallback callback) {
        value_changed_callback = callback;
    }

protected:
    void notify_value_changed() {
        if(value_changed_callback) {
            value_changed_callback(this);
        }
    }

private:
    ValueChangedCallback value_changed_callback;
};

} // namespace SignalGen
