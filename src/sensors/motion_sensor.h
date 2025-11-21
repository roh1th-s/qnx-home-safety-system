/*
 * motion_sensor.h - PIR Motion Sensor
 * 
 * This module handles reading motion detection from a PIR (Passive Infrared)
 * motion sensor. The sensor outputs HIGH when motion is detected.
 */

#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <stdbool.h>
#include "../common/public/rpi_gpio.h"

/**
 * Initialize PIR motion sensor
 * 
 * @param gpio_pin GPIO pin connected to PIR sensor output
 * @return 0 on success, -1 on error
 */
static int motion_sensor_init(int gpio_pin) {
    // Configure as input
    if (rpi_gpio_setup(gpio_pin, GPIO_IN)) {
        return -1;
    }
    return 0;
}

/**
 * Read motion detection status from PIR sensor
 * 
 * PIR sensors typically output HIGH when motion is detected.
 * Most PIR modules have adjustable sensitivity and delay time.
 * 
 * @param gpio_pin GPIO pin connected to PIR sensor
 * @param detected Pointer to store detection status (true if motion detected)
 * @return 0 on success, -1 on error
 */
static int motion_sensor_read(int gpio_pin, bool *detected) {
    unsigned level = 0;
    
    if (rpi_gpio_input(gpio_pin, &level)) {
        return -1;
    }
    
    // PIR output goes HIGH when motion is detected
    *detected = (level == GPIO_HIGH);
    
    return 0;
}

/**
 * Check if motion is currently detected
 * 
 * @param gpio_pin GPIO pin connected to PIR sensor
 * @return true if motion detected, false otherwise (or on error)
 */
static bool motion_sensor_is_detected(int gpio_pin) {
    bool detected = false;
    motion_sensor_read(gpio_pin, &detected);
    return detected;
}

#endif // MOTION_SENSOR_H

