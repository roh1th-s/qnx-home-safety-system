/*
 * alert_mgr.c
 *
 *  Created on: 19-Nov-2025
 *      Author: saura
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>

#include "alert_pulse_def.h"
#include "common/public/rpi_gpio.h"

#define LED_PIN GPIO16

int main(void)
{
    name_attach_t *attach;
    struct _pulse pulse;
    int rcvid;
    
    printf("Starting Alert Manager.......\n");
    attach = name_attach(NULL, "alert_manager", 0);
    if (attach == NULL)
    {
        perror("name_attach failed");
        exit(EXIT_FAILURE);
    }
    printf("Alert Manager waiting for pulses at /alert_manager\n");
    
    if (rpi_gpio_setup(LED_PIN, GPIO_OUT) != 0)
    {
        perror("GPIO Setup Failed");
        exit(EXIT_FAILURE);
    }
    rpi_gpio_output(LED_PIN, GPIO_LOW);

    while (1)
    {
        rcvid = MsgReceive(attach->chid, &pulse, sizeof(pulse), NULL);
        if (rcvid == -1)
        {
            perror("MsgReceive");
            continue;
        }
        if (rcvid == 0)
        {
            switch (pulse.code)
            {
            case MOTION_DETECTED:
                printf("Alert Manager: MOTION DETECTED → LED ON\n");
                rpi_gpio_output(LED_PIN, GPIO_HIGH);
                sleep(2);
                rpi_gpio_output(LED_PIN, GPIO_LOW);
                break;

            case HIGH_CO2:
                printf("Alert Manager: HIGH CO2/GAS LEVEL → LED ON\n");
                rpi_gpio_output(LED_PIN, GPIO_HIGH);
                sleep(5);
                rpi_gpio_output(LED_PIN, GPIO_LOW);
                break;

            case HIGH_TEMP:
                printf("Alert Manager: HIGH TEMPERATURE → LED ON\n");
                rpi_gpio_output(LED_PIN, GPIO_HIGH);
                sleep(3);
                rpi_gpio_output(LED_PIN, GPIO_LOW);
                break;
            
            case DOOR_OPEN:
                printf("Alert Manager: DOOR OPEN → LED ON\n");
                rpi_gpio_output(LED_PIN, GPIO_HIGH);
                sleep(3);
                rpi_gpio_output(LED_PIN, GPIO_LOW);
                break;

            default:
                printf("Alert Manager: Unknown pulse code: %d\n", pulse.code);
                break;
            }
        }
        else
        {
            MsgReply(rcvid, EOK, NULL, 0);
        }
    }
    name_detach(attach, 0);
    return 0;
}
