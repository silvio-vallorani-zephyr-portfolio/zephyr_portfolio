/*
 * Copyright (c) 2024-2025 Silvio Vallorani
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USR_THREAD_H_
#define USR_THREAD_H_ 1

#include <zephyr/kernel.h>
#include <zephyr/sys/mem_manage.h>
#include <zephyr/app_memory/app_memdomain.h>

#define _userspace_data K_APP_DMEM(userspace_partition)
#define _userspace_bss K_APP_BMEM(userspace_partition)

void thread_userspace_init(void);

#endif /* USR_THREAD_H_ */
