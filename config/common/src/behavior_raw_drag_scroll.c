#define DT_DRV_COMPAT zmk_behavior_raw_drag_scroll

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <raw_hid/events.h>


static uint8_t drag_scroll_on[32] = {
    0x53,
};

static uint8_t drag_scroll_off[32] = {
    0x73,
};

static int on_drag_scroll_pressed(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {
    raise_raw_hid_sent_event(
        (struct raw_hid_sent_event){
            .data = drag_scroll_on,
            .length = sizeof(drag_scroll_on),
        }
    );

    return ZMK_EV_EVENT_BUBBLE;
}

static int on_drag_scroll_released(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {
    raise_raw_hid_sent_event(
        (struct raw_hid_sent_event){
            .data = drag_scroll_off,
            .length = sizeof(drag_scroll_off),
        }
    );

    return ZMK_EV_EVENT_BUBBLE;
}

static const struct behavior_driver_api drag_scroll_driver_api = {
    .binding_pressed = on_drag_scroll_pressed,
    .binding_released = on_drag_scroll_released,
};

static int drag_scroll_init(const struct device *dev) {
    return 0;
}

#define DRAG_SCROLL_INST(n) \
    BEHAVIOR_DT_INST_DEFINE( \
        n, \
        drag_scroll_init, \
        NULL, \
        NULL, \
        NULL, \
        POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
        &drag_scroll_driver_api \
    );

DT_INST_FOREACH_STATUS_OKAY(DRAG_SCROLL_INST)