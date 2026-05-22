/*
 * Example: External application using bb_hal_led directly.
 *
 * Build on board (only link the module you need):
 *   gcc -I/opt/building-blocks/include -o led_example led_example.c \
 *       /opt/building-blocks/obj/bb_hal_led.o -lpthread
 *
 * Or cross-compile:
 *   aarch64-linux-gnu-gcc -Ilibbb -o led_example examples/led_example.c \
 *       build/obj/bb_hal_led.o -static -lpthread
 *
 * If you need multiple HAL modules, just list them all:
 *   ... build/obj/bb_hal_led.o build/obj/bb_hal_gpio.o build/obj/bb_json.o
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include "bb_hal_led.h"

int main(void) {
    bb_led_t led;

    if (bb_led_open(&led, "heartbeat") < 0) {
        fprintf(stderr, "Failed to open heartbeat LED\n");
        return 1;
    }
    printf("LED opened: max_brightness=%d\n", led.max_brightness);

    bb_led_set_trigger(&led, "none");

    // Blink 5 times
    for (int i = 0; i < 5; i++) {
        printf("ON\n");
        bb_led_on(&led);
        usleep(300000);

        printf("OFF\n");
        bb_led_off(&led);
        usleep(300000);
    }

    printf("Done\n");
    return 0;
}
