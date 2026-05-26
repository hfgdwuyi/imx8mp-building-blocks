#ifndef BB_HAL_PWM_H
#define BB_HAL_PWM_H
#include <stdint.h>

// PWM abstraction over /sys/class/pwm/pwmchipX/

typedef struct {
    int  chip;          // PWM chip number
    int  channel;       // PWM channel number
    int  exported;      // 1 if we exported this channel
} bb_pwm_t;

// Open PWM chip + channel. chip=0, channel=0 for pwmchip0/pwm0.
// Returns 0 on success, -1 on error.
int  bb_pwm_open(bb_pwm_t *pwm, int chip, int channel);

// Set period in nanoseconds
int  bb_pwm_set_period(bb_pwm_t *pwm, uint32_t period_ns);

// Set duty cycle in nanoseconds (must be <= period)
int  bb_pwm_set_duty(bb_pwm_t *pwm, uint32_t duty_ns);

// Enable / disable
int  bb_pwm_enable(bb_pwm_t *pwm);
int  bb_pwm_disable(bb_pwm_t *pwm);

// Close and unexport
void bb_pwm_close(bb_pwm_t *pwm);

#endif
