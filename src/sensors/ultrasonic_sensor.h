/*
 * ultrasonic_sensor.h
 *
 * This module handles reading distance data from
 * an ultrasonic sensor connected via GPIO.
 *
 * Adapted from ultrasonic sensor example from
 * https://gitlab.com/qnx/projects/hardware-component-samples
 *
 */

#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include "../common/public/rpi_gpio.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define SPEED_OF_SOUND_CM_PER_US 0.0343
#define EDGE_TIMEOUT_MS 50.0

#define GPIO_PERIPHERAL_BASE 0xfe000000

extern volatile uint32_t *__RPI_GPIO_REGS;

static float calculate_elapsed_time_us(const struct timespec *start, const struct timespec *stop)
{
    return (stop->tv_sec - start->tv_sec) * 1e6 + (stop->tv_nsec - start->tv_nsec) / 1e3;
}

static int wait_for_gpio_state(int gpio_pin, bool desired_state, double timeout_ms,
                               struct timespec *timestamp)
{
    struct timespec start, current;
    clock_gettime(CLOCK_REALTIME, &start);
    while (1)
    {
        bool current_state = (rpi_gpio_read(gpio_pin) != 0);
        if (current_state == desired_state)
        {
            clock_gettime(CLOCK_REALTIME, timestamp);
            return 0;
        }
        clock_gettime(CLOCK_REALTIME, &current);
        double elapsed_ms =
            (current.tv_sec - start.tv_sec) * 1000.0 + (current.tv_nsec - start.tv_nsec) / 1e6;
        if (elapsed_ms > timeout_ms)
        {
            return -1;
        }
    }
}

static int ultrasonic_send_pulse(int trig_pin)
{
    rpi_gpio_set(trig_pin);
    struct timespec pulse_duration = {.tv_sec = 0, .tv_nsec = 10 * 1000};
    nanosleep(&pulse_duration, NULL);
    rpi_gpio_clear(trig_pin);
    return 0;
}

static int ultrasonic_sensor_init(int trig_pin, int echo_pin)
{
    // Initialize GPIO registers if not already done
    if (__RPI_GPIO_REGS == NULL)
    {
        if (!rpi_gpio_map_regs(GPIO_PERIPHERAL_BASE))
        {
            return -1;
        }
    }

    // Set clock period for precise timing
    struct _clockperiod period;
    period.nsec = 10000;
    period.fract = 0;
    ClockPeriod(CLOCK_REALTIME, NULL, &period, 0);

    // Configure the trigger pin as an output and the echo pin as an input
    rpi_gpio_set_select(trig_pin, RPI_GPIO_FUNC_OUT);
    rpi_gpio_set_select(echo_pin, RPI_GPIO_FUNC_IN);

    // Disable the internal pull resistor on the echo pin
    rpi_gpio_set_pud_bcm2711(echo_pin, RPI_GPIO_PUD_OFF);

    return 0;
}

static int ultrasonic_sensor_read(int trig_pin, int echo_pin, uint16_t *distance_cm)
{
    float distance_float;

    if (ultrasonic_send_pulse(trig_pin) != 0)
    {
        return -1;
    }

    struct timespec rising_edge_time, falling_edge_time;

    if (wait_for_gpio_state(echo_pin, true, EDGE_TIMEOUT_MS, &rising_edge_time) != 0)
    {
        return -1;
    }

    if (wait_for_gpio_state(echo_pin, false, EDGE_TIMEOUT_MS, &falling_edge_time) != 0)
    {
        return -1;
    }

    float pulse_duration_us = calculate_elapsed_time_us(&rising_edge_time, &falling_edge_time);
    distance_float = (pulse_duration_us * SPEED_OF_SOUND_CM_PER_US) / 2.0;
    *distance_cm = (uint16_t)distance_float;

    return 0;
}

#endif
