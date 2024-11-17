#pragma once

#include "menu_item.hpp"
#include <gui/elements.h>
#include <furi/core/string.h>
#include <cmath>

namespace SignalGen {

/**
 * @brief Simple line menu item that just displays text
 */
class SimpleLineMenuItem : public MenuItem {
public:
    explicit SimpleLineMenuItem(const char* name) {
        name_ = strdup(name);
    }

    ~SimpleLineMenuItem() {
        if(name_) free(name_);
    }

    void draw(Canvas* canvas, uint8_t y, bool selected) override {
        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            elements_slightly_rounded_box(canvas, 0, y, 128, 16);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 6, y + 9, AlignLeft, AlignCenter, name_);
    }

    bool handle_input(InputEvent*) override {
        return false;
    }
    void on_enter() override {
    }
    void on_exit() override {
    }
    const char* get_name() const override {
        return name_;
    }
    const char* get_value() const override {
        return "";
    }

private:
    char* name_;
};

/**
 * @brief Number value menu item for editing numeric values
 */
class NumberValueMenuItem : public MenuItem {
public:
    NumberValueMenuItem(
        const char* name,
        int32_t min_value,
        int32_t max_value,
        int32_t step = 1,
        const char* units = "")
        : value_(min_value)
        , min_(min_value)
        , max_(max_value)
        , step_(step)
        , edit_mode_(false) {
        name_ = strdup(name);
        units_ = units ? strdup(units) : strdup("");
        value_str_[0] = '\0';
        update_value_str();
    }

    ~NumberValueMenuItem() {
        if(name_) free(name_);
        if(units_) free(units_);
    }

    void draw(Canvas* canvas, uint8_t y, bool selected) override {
        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            elements_slightly_rounded_box(canvas, 0, y, 128, 16);
            canvas_set_color(canvas, ColorWhite);
        }

        // Draw name
        canvas_draw_str_aligned(canvas, 6, y + 9, AlignLeft, AlignCenter, name_);

        // Draw value
        canvas_draw_str_aligned(canvas, 100, y + 9, AlignCenter, AlignCenter, value_str_);

        // Draw edit indicators if in edit mode
        if(edit_mode_ && selected) {
            canvas_draw_str_aligned(canvas, 80, y + 9, AlignCenter, AlignCenter, "<");
            canvas_draw_str_aligned(canvas, 120, y + 9, AlignCenter, AlignCenter, ">");
        }
    }

    bool handle_input(InputEvent* event) override {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
            return false;
        }

        if(event->key == InputKeyOk) {
            edit_mode_ = !edit_mode_;
            return true;
        }

        if(!edit_mode_) {
            return false;
        }

        bool value_changed = false;
        if(event->key == InputKeyRight) {
            if(value_ + step_ <= max_) {
                value_ += step_;
                value_changed = true;
            }
        } else if(event->key == InputKeyLeft) {
            if(value_ - step_ >= min_) {
                value_ -= step_;
                value_changed = true;
            }
        }

        if(value_changed) {
            update_value_str();
            notify_value_changed();
            return true;
        }

        return false;
    }

    void on_enter() override {
    }
    void on_exit() override {
        edit_mode_ = false;
    }
    const char* get_name() const override {
        return name_;
    }
    const char* get_value() const override {
        return value_str_;
    }

    int32_t get_numeric_value() const {
        return value_;
    }
    void set_value(int32_t value) {
        value_ = value < min_ ? min_ : (value > max_ ? max_ : value);
        update_value_str();
        notify_value_changed();
    }

private:
    void update_value_str() {
        snprintf(value_str_, sizeof(value_str_), "%ld%s", value_, units_);
    }

    char* name_;
    char* units_;
    char value_str_[32];
    int32_t value_;
    int32_t min_;
    int32_t max_;
    int32_t step_;
    bool edit_mode_;
};

/**
 * @brief Choice menu item for selecting from a list of options
 */
class ChoiceMenuItem : public MenuItem {
public:
    static constexpr size_t MAX_CHOICES = 16;

    ChoiceMenuItem(const char* name, const char* const choices[], size_t num_choices)
        : selected_index_(0) {
        name_ = strdup(name);
        num_choices_ = num_choices > MAX_CHOICES ? MAX_CHOICES : num_choices;

        for(size_t i = 0; i < num_choices_; i++) {
            choices_[i] = strdup(choices[i]);
        }
    }

    ~ChoiceMenuItem() {
        if(name_) free(name_);
        for(size_t i = 0; i < num_choices_; i++) {
            if(choices_[i]) free(choices_[i]);
        }
    }

    void draw(Canvas* canvas, uint8_t y, bool selected) override {
        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            elements_slightly_rounded_box(canvas, 0, y, 128, 16);
            canvas_set_color(canvas, ColorWhite);
        }

        // Draw name
        canvas_draw_str_aligned(canvas, 6, y + 9, AlignLeft, AlignCenter, name_);

        // Draw value with arrows
        if(num_choices_ > 0) {
            if(selected_index_ > 0) {
                canvas_draw_str_aligned(canvas, 80, y + 9, AlignCenter, AlignCenter, "<");
            }
            canvas_draw_str_aligned(
                canvas, 100, y + 9, AlignCenter, AlignCenter, choices_[selected_index_]);
            if(selected_index_ < num_choices_ - 1) {
                canvas_draw_str_aligned(canvas, 120, y + 9, AlignCenter, AlignCenter, ">");
            }
        }
    }

    bool handle_input(InputEvent* event) override {
        if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
            return false;
        }

        bool value_changed = false;
        if(event->key == InputKeyRight && selected_index_ < num_choices_ - 1) {
            selected_index_++;
            value_changed = true;
        } else if(event->key == InputKeyLeft && selected_index_ > 0) {
            selected_index_--;
            value_changed = true;
        }

        if(value_changed) {
            notify_value_changed();
            return true;
        }

        return false;
    }

    void on_enter() override {
    }
    void on_exit() override {
    }
    const char* get_name() const override {
        return name_;
    }
    const char* get_value() const override {
        return num_choices_ > 0 ? choices_[selected_index_] : "";
    }

    size_t get_selected_index() const {
        return selected_index_;
    }
    void set_selected_index(size_t index) {
        if(index < num_choices_) {
            selected_index_ = index;
            notify_value_changed();
        }
    }

private:
    char* name_;
    char* choices_[MAX_CHOICES];
    size_t num_choices_;
    size_t selected_index_;
};

} // namespace SignalGen
