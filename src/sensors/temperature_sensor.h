/*
 * temperature_sensor.h - DHT11 Temperature and Humidity Sensor
 * 
 * This module handles reading temperature and humidity data from
 * a DHT11 sensor connected via GPIO.
 */

#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include "../common/public/rpi_gpio.h"

// DHT11 timing helper functions
static inline uint64_t dht_cycles_per_usec(void) {
    return SYSPAGE_ENTRY(qtime)->cycles_per_sec / 1000000ULL;
}

static inline uint64_t dht_get_cycles(void) {
    return ClockCycles();
}

static inline void dht_delay_us(unsigned usec) {
    uint64_t start = dht_get_cycles();
    uint64_t target = start + (uint64_t)usec * dht_cycles_per_usec();
    while (dht_get_cycles() < target) {
        // busy wait
    }
}

static int dht_wait_while_level(int gpio_pin, unsigned expected, int timeout_us) {
    uint64_t start = dht_get_cycles();
    uint64_t timeout_cycles = (uint64_t)timeout_us * dht_cycles_per_usec();
    unsigned level;
    
    for (;;) {
        if (rpi_gpio_input(gpio_pin, &level)) {
            return -1;
        }
        if (level != expected) {
            uint64_t now = dht_get_cycles();
            uint64_t diff = now - start;
            return (int)(diff / dht_cycles_per_usec());
        }
        if ((dht_get_cycles() - start) > timeout_cycles) {
            return -1;
        }
    }
    return 0;
}

static int dht_read_bit(int gpio_pin) {
    int t;
    
    // Each bit starts with ~50 us LOW from sensor
    t = dht_wait_while_level(gpio_pin, GPIO_LOW, 100);
    if (t < 0) return -1;
    
    // Then HIGH: ~26-28 us -> 0, ~70 us -> 1
    t = dht_wait_while_level(gpio_pin, GPIO_HIGH, 120);
    if (t < 0) return -1;
    
    // Threshold around 50 us
    return (t > 50) ? 1 : 0;
}

/**
 * Initialize DHT11 sensor
 * 
 * @param gpio_pin GPIO pin connected to DHT11 DATA line
 * @return 0 on success, -1 on error
 */
static int temperature_sensor_init(int gpio_pin) {
    if (rpi_gpio_setup(gpio_pin, GPIO_OUT)) {
        return -1;
    }
    if (rpi_gpio_output(gpio_pin, GPIO_HIGH)) {
        return -1;
    }
    return 0;
}

/**
 * Read temperature and humidity from DHT11 sensor
 * 
 * @param gpio_pin GPIO pin connected to DHT11 DATA line
 * @param temperature Pointer to store temperature value (°C)
 * @param humidity Pointer to store humidity value (%)
 * @return 0 on success, -1 on error
 */
static int temperature_sensor_read(int gpio_pin, int *temperature, int *humidity) {
    uint8_t data[5] = {0};
    int i, bit;
    
    // 1) Start signal: MCU pulls line LOW ≥18 ms, then HIGH 20-40 us
    if (rpi_gpio_setup(gpio_pin, GPIO_OUT)) {
        return -1;
    }
    
    if (rpi_gpio_output(gpio_pin, GPIO_LOW)) {
        return -1;
    }
    usleep(20000);  // 20 ms
    
    if (rpi_gpio_output(gpio_pin, GPIO_HIGH)) {
        return -1;
    }
    dht_delay_us(40);   // 20-40 us
    
    // 2) Switch to input, sensor responds: LOW 80 us, HIGH 80 us
    if (rpi_gpio_setup(gpio_pin, GPIO_IN)) {
        return -1;
    }
    
    // Wait for sensor to pull LOW
    if (dht_wait_while_level(gpio_pin, GPIO_HIGH, 200) < 0) {
        return -1;
    }
    // LOW ~80 us
    if (dht_wait_while_level(gpio_pin, GPIO_LOW, 200) < 0) {
        return -1;
    }
    // HIGH ~80 us
    if (dht_wait_while_level(gpio_pin, GPIO_HIGH, 200) < 0) {
        return -1;
    }
    
    // 3) Read 40 bits (5 bytes)
    for (i = 0; i < 40; ++i) {
        bit = dht_read_bit(gpio_pin);
        if (bit < 0) {
            return -1;
        }
        data[i / 8] <<= 1;
        data[i / 8] |= (uint8_t)bit;
    }
    
    // 4) Verify checksum: d[4] == sum(d[0..3])
    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) {
        return -1;
    }
    
    // DHT11 format: RH int, RH dec, T int, T dec, checksum
    if (humidity)    *humidity = (int)data[0];
    if (temperature) *temperature = (int)data[2];
    
    return 0;
}

#endif // TEMPERATURE_SENSOR_H

