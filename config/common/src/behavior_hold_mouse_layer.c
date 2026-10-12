#define DT_DRV_COMPAT zmk_behavior_hold_mouse_layer

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/sys/util.h>
#include "behavior_mouse_layer.h"

struct hold_mouse_layer_config {
    zmk_keymap_layer_id_t mouse_layer;
};

static int manual_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
    const struct hold_mouse_layer_config *cfg = dev->config;
    behavior_hold_mouse_layer_update(cfg->mouse_layer, true);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int manual_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
    const struct hold_mouse_layer_config *cfg = dev->config;
    behavior_hold_mouse_layer_update(cfg->mouse_layer, false);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api manual_api = {
    .binding_pressed = manual_pressed,
    .binding_released = manual_released,
};

static int manual_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define HOLD_MOUSE_LAYER_INST(n) \
    static const struct hold_mouse_layer_config hold_config_##n = { \
        .mouse_layer = DT_INST_PROP(n, mouse_layer), \
    }; \
    BEHAVIOR_DT_INST_DEFINE( \
        n, manual_init, NULL, NULL, &hold_config_##n, POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &manual_api \
    );

DT_INST_FOREACH_STATUS_OKAY(HOLD_MOUSE_LAYER_INST)
