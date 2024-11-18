#include "signal_gen_pwm_cpp_impl.hpp"
#include <furi_hal.h>
#include <gui/elements.h>
#include <memory>

using namespace SignalGen;

static const char* const PWM_CH_NAMES[] = {"2(A7)", "4(A4)"};
static constexpr uint32_t FREQ_MAX = 1000000UL;
static constexpr uint8_t DUTY_MAX = 100;

struct PwmViewModel {
    PwmView* pwm;
};

PwmView::PwmView()
    : menu(nullptr)
    , callback(nullptr)
    , callback_context(nullptr)
    , channel_item(nullptr)
    , freq_item(nullptr)
    , duty_item(nullptr) {
    view = view_alloc();
    view_set_context(view, this);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(PwmViewModel));
    with_view_model_cpp(view, PwmViewModel*, model, { model->pwm = this; }, true);

    menu = new Menu();
    setup_menu();

    //   set_params(0, 1022, 54);
    view_set_draw_callback(view, draw_callback);
    view_set_input_callback(view, input_callback);
}

PwmView::~PwmView() {
    delete menu;
    delete channel_item;
    delete duty_item;
    delete freq_item;
    view_free(view);
}

void PwmView::setup_menu() {
    // Channel selection
    channel_item = new ChoiceMenuItem("GPIO YOLO Pin", PWM_CH_NAMES, 2);
    duty_item = new NumberValueMenuItem("Duty Cycle", 0, DUTY_MAX, 1, "%");
    freq_item = new NumberValueMenuItem("Frequency", 1, FREQ_MAX, 1, "Hz");

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
    menu->add_item(channel_item);
    menu->add_item(freq_item);
    menu->add_item(duty_item);

    set_params(0, 1003, 50);
}

void PwmView::set_callback(SignalGenPwmViewCallback cb, void* context) {
    callback = cb;
    callback_context = context;
}

void PwmView::set_params(uint8_t channel_id, uint32_t freq, uint8_t duty) {
    furi_assert(channel_item);
    channel_item->set_selected_index(channel_id);
    furi_assert(freq_item);
    freq_item->set_value(freq);
    furi_assert(duty_item);
    duty_item->set_value(duty);
}

void PwmView::draw_callback(Canvas* canvas, void* _model) {
    auto pwmModel = static_cast<PwmViewModel*>(_model);
    furi_assert(pwmModel);
    furi_assert(pwmModel->pwm);

    pwmModel->pwm->menu->draw(canvas);
}

bool PwmView::input_callback(InputEvent* event, void* context) {
    auto pwm = static_cast<PwmView*>(context);
    furi_assert(pwm);
    bool result = pwm->menu->handle_input(event);
    // result = true;
    return result;
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
    furi_assert(pwm);
    furi_assert(pwm->impl);
    pwm->impl->set_params(channel_id, freq, duty);
}
}
