/*
 * Copyright (c) 2024-2025 Silvio Vallorani
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/mem_manage.h>
#include <zephyr/sys/libc-hooks.h>
#include <zephyr/sys/util.h>
#include <zephyr/app_memory/app_memdomain.h>
#include "usr_thread.h"

//#define FORCE_TEST_USERSPACE_MEM_ACCESS // Uncomment to test the user mode access to the memory domain
//#define FORCE_TEST_USERSPACE_GPIO_ACCESS // Uncomment to test the user mode access to the GPIO device

// DEBUG PIN DEFINITION
static const struct gpio_dt_spec dbg_pin1 = GPIO_DT_SPEC_GET(DT_ALIAS(dbg_pin1), gpios);

// MEMORY PARTITION DEFINITION
FOR_EACH(K_APPMEM_PARTITION_DEFINE, (;), userspace_partition);
struct k_mem_partition *userspace_parts[] = {
#if Z_LIBC_PARTITION_EXISTS
	&z_libc_partition,
#endif
	&userspace_partition,
};
struct k_mem_domain userspace_domain;

// THREAD USERSPACE DEFINITIONS
#define THREAD_USERSPACE_SLEEPTIME 1000
#define THREAD_USERSPACE_STACKSIZE 2048
#define THREAD_USERSPACE_PRIORITY 7
#define THREAD_USERSPACE_OPTIONS K_USER
#define THREAD_USERSPACE_DEALAYED_STARTUP_MS K_FOREVER
// Define the stack for the new thread
K_THREAD_STACK_DEFINE(thread_userspace_stack_area, THREAD_USERSPACE_STACKSIZE);
// Define the thread data structure
struct k_thread thread_userspace;
// Define the thread entry point
static void thread_userspace_loop(void *dummy1, void *dummy2, void *dummy3);
// Define the thread ID
k_tid_t userspace_thread;

//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD USERSPACE INITIALIZATION
//////////////////////////////////////////////////////////////////////////////////////////
void thread_userspace_init(void) {
    __unused int ret;
    // Setup debug pins
    ret = gpio_is_ready_dt(&dbg_pin1);
    ret = gpio_pin_configure_dt(&dbg_pin1, GPIO_OUTPUT_ACTIVE);

    // Init the userspace memory domain
    ret = k_mem_domain_init(&userspace_domain, ARRAY_SIZE(userspace_parts), userspace_parts);

	// Create the userspace thread in users mode
	userspace_thread = k_thread_create(&thread_userspace, thread_userspace_stack_area, THREAD_USERSPACE_STACKSIZE,
					thread_userspace_loop, NULL, NULL, NULL,
					THREAD_USERSPACE_PRIORITY, THREAD_USERSPACE_OPTIONS, THREAD_USERSPACE_DEALAYED_STARTUP_MS);

    // Add the userspace thread to the userspace memory domain
    ret = k_mem_domain_add_thread(&userspace_domain, userspace_thread);

    // Grant access to the GPIO device to the userspace thread
#if defined(FORCE_TEST_USERSPACE_GPIO_ACCESS)
#else
    k_object_access_grant(dbg_pin1.port, userspace_thread);
#endif

	// Start the userspace thread
	k_thread_start(userspace_thread);
}

//////////////////////////////////////////////////////////////////////////////////////////
//  USERSPACE GLOBAL VARIABLES DECLARATION
//////////////////////////////////////////////////////////////////////////////////////////
_userspace_bss int foo;
#if defined(FORCE_TEST_USERSPACE_MEM_ACCESS)
int bar;
#else
_userspace_bss int bar;
#endif

//////////////////////////////////////////////////////////////////////////////////////////
//  THREAD USERSPACE IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////
static void thread_userspace_loop(void *dummy1, void *dummy2, void *dummy3) {
    __unused int ret;
    ret = 0;

	foo = 1;
	bar = 2; // This will generate a fault if FORCE_TEST_USERSPACE_MEM_ACCESS is defined

    while (1) {
        ret = gpio_pin_toggle_dt(&dbg_pin1); // This will generate a fault if FORCE_TEST_USERSPACE_GPIO_ACCESS is defined
        k_msleep(THREAD_USERSPACE_SLEEPTIME);
    }
    ARG_UNUSED(dummy1);
    ARG_UNUSED(dummy2);
    ARG_UNUSED(dummy3);
}
