#include "signal_gen_pwm_cpp_impl.hpp"
#include <gui/elements.h>
#include <furi_hal.h>

using namespace SignalGen;

static const char* const PWM_CH_NAMES[] = {"2(A7)", "4(A4)"};
static constexpr uint32_t FREQ_MAX = 1000000UL;
static constexpr uint8_t DUTY_MAX = 100;

PwmView::PwmView()
    : callback(nullptr)
    , callback_context(nullptr) {
    view = view_alloc();
    view_set_context(view, this);
    view_set_draw_callback(view, draw_callback);
    view_set_input_callback(view, input_callback);

    menu = std::make_unique<Menu>();
    setup_menu();
}

PwmView::~PwmView() {
    view_free(view);
}

void PwmView::setup_menu() {
    // Channel selection
    channel_item = std::make_unique<ChoiceMenuItem>("GPIO Pin", PWM_CH_NAMES, 2);
    channel_item->set_value_changed_callback([this](const MenuItem* item) {
        auto choice = static_cast<const ChoiceMenuItem*>(item);
        if(callback) {
            callback(
                choice->get_selected_index(),
                freq_item->get_numeric_value(),
                duty_item->get_numeric_value(),
                callback_context);
        }
    });

    // Frequency control
    freq_item = std::make_unique<NumberValueMenuItem>("Frequency", 1, FREQ_MAX, 1, "Hz");
    freq_item->set_value_changed_callback([this](const MenuItem* item) {
        auto number = static_cast<const NumberValueMenuItem*>(item);
        if(callback) {
            callback(
                channel_item->get_selected_index(),
                number->get_numeric_value(),
                duty_item->get_numeric_value(),
                callback_context);
        }
    });

    // Duty cycle control
    duty_item = std::make_unique<NumberValueMenuItem>("Duty Cycle", 0, DUTY_MAX, 1, "%");
    duty_item->set_value_changed_callback([this](const MenuItem* item) {
        auto number = static_cast<const NumberValueMenuItem*>(item);
        if(callback) {
            callback(
                channel_item->get_selected_index(),
                freq_item->get_numeric_value(),
                number->get_numeric_value(),
                callback_context);
        }
    });

    // Add items to menu
    menu->add_item(std::move(channel_item));
    menu->add_item(std::move(freq_item));
    menu->add_item(std::move(duty_item));
}

void PwmView::set_callback(SignalGenPwmViewCallback cb, void* context) {
    callback = cb;
    callback_context = context;
}

void PwmView::set_params(uint8_t channel_id, uint32_t freq, uint8_t duty) {
    channel_item->set_selected_index(channel_id);
    freq_item->set_value(freq);
    duty_item->set_value(duty);
}

void PwmView::draw_callback(Canvas* canvas, void* context) {
    auto pwm = static_cast<PwmView*>(context);
    pwm->menu->draw(canvas);
}

bool PwmView::input_callback(InputEvent* event, void* context) {
    auto pwm = static_cast<PwmView*>(context);
    return pwm->menu->handle_input(event);
}

// C API implementation
extern "C" {
typedef struct SignalGenPwmCpp {
    PwmView* impl;
} SignalGenPwmCpp;

SignalGenPwmCpp* signal_gen_pwm_cpp_alloc() {
    auto pwm = new SignalGenPwmCpp();
    pwm->impl = new PwmView();
    return pwm;
}

void signal_gen_pwm_cpp_free(SignalGenPwmCpp* pwm) {
    delete pwm->impl;
    delete pwm;
}

View* signal_gen_pwm_cpp_get_view(SignalGenPwmCpp* pwm) {
    return pwm->impl->get_view();
}

void signal_gen_pwm_cpp_set_callback(
    SignalGenPwmCpp* pwm,
    SignalGenPwmViewCallback callback,
    void* context) {
    pwm->impl->set_callback(callback, context);
}

void signal_gen_pwm_cpp_set_params(
    SignalGenPwmCpp* pwm,
    uint8_t channel_id,
    uint32_t freq,
    uint8_t duty) {
    pwm->impl->set_params(channel_id, freq, duty);
}
}
