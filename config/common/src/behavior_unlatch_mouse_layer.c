#define DT_DRV_COMPAT zmk_behavior_unlatch_mouse_layer

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/sys/util.h>
#include "behavior_mouse_layer.h"

struct unlatch_mouse_layer_config {
    zmk_keymap_layer_id_t mouse_layer;
    zmk_keymap_layer_id_t base_layer;
};

static int unlatch_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
    const struct unlatch_mouse_layer_config *cfg = dev->config;
    behavior_unlatch_mouse_layer(cfg->mouse_layer, cfg->base_layer);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int unlatch_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api unlatch_api = {
    .binding_pressed = unlatch_pressed,
    .binding_released = unlatch_released,
};

static int unlatch_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define UNLATCH_MOUSE_LAYER_INST(n) \
    static const struct unlatch_mouse_layer_config unlatch_config_##n = { \
        .mouse_layer = DT_INST_PROP(n, mouse_layer), \
        .base_layer = DT_INST_PROP(n, base_layer), \
    }; \
    BEHAVIOR_DT_INST_DEFINE( \
        n, unlatch_init, NULL, NULL, &unlatch_config_##n, POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &unlatch_api \
    );

DT_INST_FOREACH_STATUS_OKAY(UNLATCH_MOUSE_LAYER_INST)
