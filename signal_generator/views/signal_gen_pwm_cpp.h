#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef void (
    *SignalGenPwmViewCallback)(uint8_t channel_id, uint32_t freq, uint8_t duty, void* context);

typedef struct SignalGenPwmCpp SignalGenPwmCpp;

// C API
SignalGenPwmCpp* signal_gen_pwm_cpp_alloc();
void signal_gen_pwm_cpp_free(SignalGenPwmCpp* pwm);
View* signal_gen_pwm_cpp_get_view(SignalGenPwmCpp* pwm);
void signal_gen_pwm_cpp_set_callback(
    SignalGenPwmCpp* pwm,
    SignalGenPwmViewCallback callback,
    void* context);
void signal_gen_pwm_cpp_set_params(
    SignalGenPwmCpp* pwm,
    uint8_t channel_id,
    uint32_t freq,
    uint8_t duty);

#ifdef __cplusplus
}
#endif
