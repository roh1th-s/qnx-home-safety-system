/*
 * gas_sensor.h - MQ135 Gas Sensor
 * 
 * This module handles reading digital output from an MQ135 gas sensor.
 * The sensor detects various gases including NH3, NOx, alcohol, benzene, etc.
 */

#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <stdbool.h>
#include "../common/public/rpi_gpio.h"

/**
 * Initialize MQ135 gas sensor
 * 
 * @param gpio_pin GPIO pin connected to MQ135 D0 (digital output)
 * @return 0 on success, -1 on error
 */
static int gas_sensor_init(int gpio_pin) {
    // Configure as input; module already has pull-up on D0
    if (rpi_gpio_setup(gpio_pin, GPIO_IN)) {
        return -1;
    }
    return 0;
}

/**
 * Read gas detection status from MQ135 sensor
 * 
 * The MQ135 module's digital output (D0) goes LOW when gas concentration
 * exceeds the threshold set by the onboard potentiometer.
 * 
 * @param gpio_pin GPIO pin connected to MQ135 D0
 * @param detected Pointer to store detection status (true if gas detected)
 * @return 0 on success, -1 on error
 */
static int gas_sensor_read(int gpio_pin, bool *detected) {
    unsigned level = 0;
    
    if (rpi_gpio_input(gpio_pin, &level)) {
        return -1;
    }
    
    // On most MQ135 modules, D0 goes LOW when gas > threshold
    *detected = (level == GPIO_LOW);
    
    return 0;
}

/**
 * Check if gas is currently detected
 * 
 * @param gpio_pin GPIO pin connected to MQ135 D0
 * @return true if gas detected, false otherwise (or on error)
 */
static bool gas_sensor_is_detected(int gpio_pin) {
    bool detected = false;
    gas_sensor_read(gpio_pin, &detected);
    return detected;
}

#endif // GAS_SENSOR_H



