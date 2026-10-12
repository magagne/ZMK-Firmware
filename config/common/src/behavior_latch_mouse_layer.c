#define DT_DRV_COMPAT zmk_behavior_latch_mouse_layer

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/sys/util.h>
#include "behavior_mouse_layer.h"

static int latch_pressed(struct zmk_behavior_binding *binding,
                         struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    behavior_latch_mouse_layer_set(binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int latch_released(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api latch_api = {
    .binding_pressed = latch_pressed,
    .binding_released = latch_released,
};

static int latch_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define LATCH_MOUSE_LAYER_INST(n) \
    BEHAVIOR_DT_INST_DEFINE( \
        n, latch_init, NULL, NULL, NULL, POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &latch_api \
    );

DT_INST_FOREACH_STATUS_OKAY(LATCH_MOUSE_LAYER_INST)
