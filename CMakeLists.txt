# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_portfolio)

# option(TESTS "Run tests in test/" ON)
# option(TESTS "Run tests in test/" OFF)
if(TESTS)
    target_sources(app PRIVATE test/main.c)
else()
    target_sources(app PRIVATE src/main.c)
endif()
unset(TESTS CACHE)
