#define DT_DRV_COMPAT zmk_behavior_raw_temporary_dpi

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <raw_hid/events.h>

static uint8_t temporary_dpi_on[32] = {
    0x54,
};

static uint8_t temporary_dpi_off[32] = {
    0x74,
};

static int on_temporary_dpi_pressed(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {
    raise_raw_hid_sent_event(
        (struct raw_hid_sent_event){
            .data = temporary_dpi_on,
            .length = sizeof(temporary_dpi_on),
        }
    );

    return ZMK_EV_EVENT_BUBBLE;
}

static int on_temporary_dpi_released(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event
) {
    raise_raw_hid_sent_event(
        (struct raw_hid_sent_event){
            .data = temporary_dpi_off,
            .length = sizeof(temporary_dpi_off),
        }
    );

    return ZMK_EV_EVENT_BUBBLE;
}

static const struct behavior_driver_api temporary_dpi_driver_api = {
    .binding_pressed = on_temporary_dpi_pressed,
    .binding_released = on_temporary_dpi_released,
};

static int temporary_dpi_init(const struct device *dev) {
    return 0;
}

#define TEMPORARY_DPI_INST(n) \
    BEHAVIOR_DT_INST_DEFINE( \
        n, \
        temporary_dpi_init, \
        NULL, \
        NULL, \
        NULL, \
        POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
        &temporary_dpi_driver_api \
    );

DT_INST_FOREACH_STATUS_OKAY(TEMPORARY_DPI_INST)
