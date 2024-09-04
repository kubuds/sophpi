#!/bin/sh

echo 0 > /sys/module/cv180x_vi/parameters/bw_en
#SD1_D2->SEL GPIO 371
devmem 0x03001090 32 3

#ADC1 -> PWM
devmem 0x030010a8 32 6
insmod /mnt/system/ko/cv180x_pwm.ko
echo 3 > /sys/class/pwm/pwmchip0/export
echo 33333333 >/sys/class/pwm/pwmchip0/pwm3/period
echo 16666666 >/sys/class/pwm/pwmchip0/pwm3/duty_cycle
echo 1 >/sys/class/pwm/pwmchip0/pwm3/enable
