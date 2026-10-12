#define DT_DRV_COMPAT zmk_behavior_kp_or_latch_mouse_layer

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zephyr/sys/util.h>

struct kp_or_latch_mouse_layer_config {
    const char *hold_tap_behavior_dev;
};

static int invoke_inner(struct zmk_behavior_binding *binding,
                        struct zmk_behavior_binding_event event,
                        bool pressed) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    const struct kp_or_latch_mouse_layer_config *cfg = dev->config;
    struct zmk_behavior_binding inner = {
        .behavior_dev = cfg->hold_tap_behavior_dev,
        .param1 = 0,
        .param2 = binding->param1,
    };

    return zmk_behavior_invoke_binding(&inner, event, pressed);
}

static int wrapper_pressed(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    return invoke_inner(binding, event, true);
}

static int wrapper_released(struct zmk_behavior_binding *binding,
                            struct zmk_behavior_binding_event event) {
    return invoke_inner(binding, event, false);
}

static const struct behavior_driver_api wrapper_api = {
    .binding_pressed = wrapper_pressed,
    .binding_released = wrapper_released,
};

static int wrapper_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define KP_OR_LATCH_MOUSE_LAYER_INST(n) \
    static const struct kp_or_latch_mouse_layer_config wrapper_config_##n = { \
        .hold_tap_behavior_dev = \
            DEVICE_DT_NAME(DT_INST_PHANDLE(n, hold_tap_behavior)), \
    }; \
    BEHAVIOR_DT_INST_DEFINE( \
        n, wrapper_init, NULL, NULL, &wrapper_config_##n, POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &wrapper_api \
    );

DT_INST_FOREACH_STATUS_OKAY(KP_OR_LATCH_MOUSE_LAYER_INST)
