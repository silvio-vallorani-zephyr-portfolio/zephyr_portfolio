/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/smf.h>
#include "tl_smf.h"

// TRAFFICLIGHT DEFINITIONS
#define TF_LIGHT_ON  0
#define TF_LIGHT_OFF 1

// TRAFFICLIGHT PINS DEFINITIONS
static const struct gpio_dt_spec tl_red = GPIO_DT_SPEC_GET(DT_ALIAS(tl_red), gpios);
static const struct gpio_dt_spec tl_orange = GPIO_DT_SPEC_GET(DT_ALIAS(tl_orange), gpios);
static const struct gpio_dt_spec tl_green = GPIO_DT_SPEC_GET(DT_ALIAS(tl_green), gpios);

// THREAD TRAFFICLIGHT DEFINITIONS
#define THREAD_TRAFFICLIGHT_SLEEPTIME 500
#define THREAD_TRAFFICLIGHT_STACKSIZE 2048
#define THREAD_TRAFFICLIGHT_PRIORITY 7
void thread_trafficlight_entry_point(void *dummy1, void *dummy2, void *dummy3);
void thread_trafficlight_loop(const char *my_name);
K_THREAD_DEFINE(thread_trafficlight, 
                THREAD_TRAFFICLIGHT_STACKSIZE, 
                thread_trafficlight_entry_point, NULL, NULL, NULL,
                THREAD_TRAFFICLIGHT_PRIORITY, 0, 0);

void tf_timer_handler(struct k_timer *dummy);
void tl_timeout_work_handler(struct k_work *work);
K_TIMER_DEFINE(tl_timer, tf_timer_handler, NULL);
K_WORK_DEFINE(tl_timeout_work, tl_timeout_work_handler);

/* Forward declaration of state table */
static const struct smf_state tl_states[];

/* List of demo states */
enum tl_state { SERVICE, STOP, WARNING, GO };

/* User defined object */
struct s_object tl_s_obj;
/* State SERVICE */
static void service_entry(void *o) {
    /* Turn off all lights */
    gpio_pin_set_dt(&tl_red, TF_LIGHT_OFF);
    gpio_pin_set_dt(&tl_orange, TF_LIGHT_OFF);
    gpio_pin_set_dt(&tl_green, TF_LIGHT_OFF);
    k_timer_stop(&tl_timer);
}
static void service_run(void *o) {
    struct s_object *s = (struct s_object *)o;
    /* Blink orange light */
    gpio_pin_toggle_dt(&tl_orange);
    /* Change states on Button Press Event */
    if (s->events & TL_EVENT_BTN_PRESS) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[STOP]);
    }
}
static void service_exit(void *o) {
    /* Turn off orange light */
    gpio_pin_set_dt(&tl_orange, TF_LIGHT_OFF);
}

/* State STOP */
static void stop_entry(void *o) {
    /* Do something */
    /* Turn on red light */
    gpio_pin_set_dt(&tl_red, TF_LIGHT_ON);
    k_timer_start(&tl_timer, K_SECONDS(10), K_NO_WAIT);
}
static void stop_run(void *o) {
    struct s_object *s = (struct s_object *)o;
    /* Change states on Button Press Event */
    if (s->events & TL_EVENT_BTN_PRESS) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[SERVICE]);
    } else if (s->events & TL_EVENT_TIMEOUT) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[GO]);
    }
}
static void stop_exit(void *o) {
    /* Do something */
    /* Turn off red light */
    gpio_pin_set_dt(&tl_red, TF_LIGHT_OFF);
}

/* State WARNING */
static void warning_entry(void *o) {
    /* Do something */
    /* Turn on orange light */
    gpio_pin_set_dt(&tl_orange, TF_LIGHT_ON);
    k_timer_start(&tl_timer, K_SECONDS(4), K_NO_WAIT);
}
static void warning_run(void *o) {
        struct s_object *s = (struct s_object *)o;
    /* Change states on Button Press Event */
    if (s->events & TL_EVENT_BTN_PRESS) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[SERVICE]);
    } else if (s->events & TL_EVENT_TIMEOUT) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[STOP]);
    }
}
static void warning_exit(void *o) {
    /* Do something */
    /* Turn off orange light */
    gpio_pin_set_dt(&tl_orange, TF_LIGHT_OFF);
}

/* State GO */
static void go_entry(void *o) {
    /* Do something */
    /* Turn on green light */
    gpio_pin_set_dt(&tl_green, TF_LIGHT_ON);
    k_timer_start(&tl_timer, K_SECONDS(20), K_NO_WAIT);
}
static void go_run(void *o) {
        struct s_object *s = (struct s_object *)o;
    /* Change states on Button Press Event */
    if (s->events & TL_EVENT_BTN_PRESS) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[SERVICE]);
    } else if (s->events & TL_EVENT_TIMEOUT) {
        smf_set_state(SMF_CTX(&tl_s_obj), &tl_states[WARNING]);
    }
}
static void go_exit(void *o) {
    /* Do something */
    /* Turn off green light */
    gpio_pin_set_dt(&tl_green, TF_LIGHT_OFF);
}

/* Populate state table */
static const struct smf_state tl_states[] = {
    [SERVICE] = SMF_CREATE_STATE(service_entry, service_run, service_exit, NULL, NULL),
    [STOP   ] = SMF_CREATE_STATE(stop_entry, stop_run, stop_exit, NULL, NULL),
    [WARNING] = SMF_CREATE_STATE(warning_entry, warning_run, warning_exit, NULL, NULL),
    [GO     ] = SMF_CREATE_STATE(go_entry, go_run, go_exit, NULL, NULL),
};

void tf_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&tl_timeout_work);
}

void tl_timeout_work_handler(struct k_work *work)
{
    k_event_post(&tl_s_obj.smf_event, TL_EVENT_TIMEOUT);
}


//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD TRAFFICLIGHT IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
void thread_trafficlight_entry_point(void *dummy1, void *dummy2, void *dummy3) {
    __unused volatile int ret;
    // Setup debug pins
    ret = gpio_is_ready_dt(&tl_red);
    ret = gpio_pin_configure_dt(&tl_red, GPIO_OUTPUT_ACTIVE);
    ret = gpio_is_ready_dt(&tl_orange);
    ret = gpio_pin_configure_dt(&tl_orange, GPIO_OUTPUT_ACTIVE);
    ret = gpio_is_ready_dt(&tl_green);
    ret = gpio_pin_configure_dt(&tl_green, GPIO_OUTPUT_ACTIVE);
    k_event_init(&tl_s_obj.smf_event);
    smf_set_initial(SMF_CTX(&tl_s_obj), &tl_states[SERVICE]);
    thread_trafficlight_loop(__func__);
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
}

void tl_wdt_callback (int channel_id, void *user_data);
void thread_trafficlight_loop(const char *my_name) {
    __unused volatile int ret;
    int task_wdt_id = task_wdt_add(2*THREAD_TRAFFICLIGHT_SLEEPTIME, tl_wdt_callback, (void *)k_current_get());
    while (1) {
        // Feed the task watchdog
        task_wdt_feed(task_wdt_id);
        tl_s_obj.events = k_event_wait(&tl_s_obj.smf_event, TL_EVENT_BTN_PRESS | TL_EVENT_TIMEOUT, true, K_MSEC(THREAD_TRAFFICLIGHT_SLEEPTIME));
        ret = smf_run_state(SMF_CTX(&tl_s_obj));
        if (ret) {
            k_cpu_idle();
            //break;
        }
    }
    ARG_UNUSED(my_name);
}


//////////////////////////////////////////////////////////////////////////////////////////
//  TASK WATCHDOG IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
void tl_wdt_callback(int channel_id, void *user_data)
{
    sys_reboot(SYS_REBOOT_WARM);
}
