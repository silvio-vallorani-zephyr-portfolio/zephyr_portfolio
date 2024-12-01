#ifndef TL_SMF_H_
#define TL_SMF_H_

#include <zephyr/kernel.h>
#include <zephyr/smf.h>

// TRAFFICLIGHT EVENTS DEFINITIONS
#define TL_EVENT_BTN_PRESS BIT(0)
#define TL_EVENT_TIMEOUT  BIT(1)

struct s_object {
    /* This must be first */
    struct smf_ctx ctx;
    /* Other state specific data add here */
    /* Events */
    struct k_event smf_event;
    int32_t events;
};

#endif /* TL_SMF_H_ */