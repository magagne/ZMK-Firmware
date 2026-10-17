#define DT_DRV_COMPAT zmk_behavior_raw_temporary_dpi

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/keys.h>
#include <raw_hid/events.h>
#include <zmk/behavior.h>
#include <zmk/events/keycode_state_changed.h>

LOG_MODULE_REGISTER(behavior_raw_temporary_dpi, CONFIG_ZMK_LOG_LEVEL);

#define WINDOWS_TOGGLE_PULSE_MS 8
#define WINDOWS_RETRY_DELAY_MS 50
#define WINDOWS_PENDING_CAPACITY 8

struct temporary_dpi_config {
    bool windows_host;
};

static uint8_t temporary_dpi_on[32] = { 0x54 };
static uint8_t temporary_dpi_off[32] = { 0x74 };

/*
 * Mac request state is protected by a spinlock. Raw HID output and the
 * output-active state are owned by the serialized work handler.
 */
static struct k_spinlock mac_lock;
static bool mac_manual_requested;
static bool mac_standalone_requested;
static bool mac_output_active;

static void mac_reconcile_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(mac_reconcile_work, mac_reconcile_work_handler);

static void mac_queue_reconcile(void) {
    int rc = k_work_reschedule(&mac_reconcile_work, K_NO_WAIT);

    if (rc < 0) {
        LOG_ERR("Mac DPI reconcile scheduling failed: %d; retrying", rc);
        rc = k_work_reschedule(
            &mac_reconcile_work, K_MSEC(WINDOWS_RETRY_DELAY_MS));
        if (rc < 0) {
            LOG_ERR("Mac DPI delayed reconcile scheduling failed: %d", rc);
        }
    }
}

static void mac_set_request(bool standalone, bool active) {
    k_spinlock_key_t key = k_spin_lock(&mac_lock);

    if (standalone) {
        mac_standalone_requested = active;
    } else {
        mac_manual_requested = active;
    }

    k_spin_unlock(&mac_lock, key);
    mac_queue_reconcile();
}

static void mac_reconcile_work_handler(struct k_work *work) {
    (void)work;

    k_spinlock_key_t key = k_spin_lock(&mac_lock);
    bool desired = mac_manual_requested || mac_standalone_requested;
    k_spin_unlock(&mac_lock, key);

    if (desired != mac_output_active) {
        int rc = raise_raw_hid_sent_event(
            (struct raw_hid_sent_event){
                .data = desired ? temporary_dpi_on : temporary_dpi_off,
                .length = sizeof(temporary_dpi_on),
            });

        if (rc < 0) {
            LOG_ERR("Mac temporary-DPI Raw HID event failed: %d", rc);
            int retry_rc = k_work_reschedule(
                &mac_reconcile_work, K_MSEC(WINDOWS_RETRY_DELAY_MS));
            if (retry_rc < 0) {
                LOG_ERR("Mac delayed DPI retry scheduling failed: %d", retry_rc);
            }
            return;
        }

        mac_output_active = desired;
    }

    /* A request may have changed while the event was being dispatched. */
    key = k_spin_lock(&mac_lock);
    bool latest_desired = mac_manual_requested || mac_standalone_requested;
    k_spin_unlock(&mac_lock, key);

    if (latest_desired != mac_output_active) {
        mac_queue_reconcile();
    }
}

/*
 * Windows requests enqueue aggregate state transitions. This preserves a
 * quick ON then OFF even if both requests arrive before the worker executes.
 * An uncertain Num Lock toggle is never retried.
 */
static struct k_spinlock windows_lock;
static bool windows_manual_requested;
static bool windows_standalone_requested;
static bool windows_output_active;
static bool windows_output_faulted;
static bool windows_toggle_in_flight;
static bool windows_toggle_target;
static bool windows_press_succeeded;

static bool windows_pending_targets[WINDOWS_PENDING_CAPACITY];
static uint8_t windows_pending_head;
static uint8_t windows_pending_count;
static bool windows_last_queued_target;

static void windows_reconcile_work_handler(struct k_work *work);
static void windows_release_work_handler(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(windows_reconcile_work, windows_reconcile_work_handler);
K_WORK_DELAYABLE_DEFINE(windows_release_work, windows_release_work_handler);

static int windows_key_event(bool pressed) {
    return raise_zmk_keycode_state_changed_from_encoded(
        KP_NUMLOCK, pressed, k_uptime_get());
}

static void windows_queue_reconcile(void) {
    int rc = k_work_reschedule(&windows_reconcile_work, K_NO_WAIT);

    if (rc < 0) {
        LOG_ERR("Windows DPI scheduling failed: %d; retrying", rc);
        rc = k_work_reschedule(
            &windows_reconcile_work, K_MSEC(WINDOWS_RETRY_DELAY_MS));
        if (rc < 0) {
            LOG_ERR("Windows DPI delayed scheduling failed: %d", rc);
            k_spinlock_key_t key = k_spin_lock(&windows_lock);
            windows_output_faulted = true;
            k_spin_unlock(&windows_lock, key);
            LOG_ERR("Windows DPI controller faulted after scheduling failure");
        }
    }
}

static void windows_reconcile_work_handler(struct k_work *work) {
    (void)work;

    k_spinlock_key_t key = k_spin_lock(&windows_lock);

    if (windows_output_faulted || windows_toggle_in_flight ||
        windows_pending_count == 0) {
        k_spin_unlock(&windows_lock, key);
        return;
    }

    bool target = windows_pending_targets[windows_pending_head];
    windows_pending_head =
        (windows_pending_head + 1) % WINDOWS_PENDING_CAPACITY;
    windows_pending_count--;

    if (target == windows_output_active) {
        k_spin_unlock(&windows_lock, key);
        windows_queue_reconcile();
        return;
    }

    windows_toggle_target = target;
    windows_toggle_in_flight = true;
    windows_press_succeeded = false;
    k_spin_unlock(&windows_lock, key);

    int press_rc = windows_key_event(true);

    key = k_spin_lock(&windows_lock);
    windows_press_succeeded = (press_rc >= 0);
    if (press_rc < 0) {
        windows_output_faulted = true;
    }
    k_spin_unlock(&windows_lock, key);

    if (press_rc < 0) {
        LOG_ERR("Num Lock press dispatch failed; Windows DPI controller faulted");
    }

    /* Always try to release the simulated key, even after a press error. */
    int schedule_rc = k_work_reschedule(
        &windows_release_work, K_MSEC(WINDOWS_TOGGLE_PULSE_MS));

    if (schedule_rc < 0) {
        LOG_ERR("Could not schedule Num Lock release; releasing immediately");

        int release_rc = windows_key_event(false);
        if (release_rc < 0) {
            LOG_ERR("Immediate Num Lock release failed; retrying once");
            release_rc = windows_key_event(false);
        }

        key = k_spin_lock(&windows_lock);
        windows_output_faulted = true;
        windows_toggle_in_flight = false;
        k_spin_unlock(&windows_lock, key);

        if (release_rc < 0) {
            LOG_ERR("Num Lock release recovery failed");
        }
        LOG_ERR("Windows DPI controller faulted after release scheduling failure");
    }
}

static void windows_release_work_handler(struct k_work *work) {
    (void)work;

    int release_rc = windows_key_event(false);
    if (release_rc < 0) {
        LOG_ERR("Num Lock release failed; retrying once");
        release_rc = windows_key_event(false);
    }

    k_spinlock_key_t key = k_spin_lock(&windows_lock);

    bool press_succeeded = windows_press_succeeded;
    if (!press_succeeded || release_rc < 0) {
        windows_output_faulted = true;
    } else if (!windows_output_faulted) {
        windows_output_active = windows_toggle_target;
    }

    windows_toggle_in_flight = false;
    bool faulted = windows_output_faulted;
    bool pending = windows_pending_count > 0;
    k_spin_unlock(&windows_lock, key);

    if (!press_succeeded || release_rc < 0) {
        LOG_ERR("Windows DPI controller faulted; toggle outcome is uncertain");
    }

    if (!faulted && pending) {
        windows_queue_reconcile();
    }
}

static void windows_set_request(bool standalone, bool active) {
    bool overflow = false;
    bool enqueue = false;

    k_spinlock_key_t key = k_spin_lock(&windows_lock);

    if (standalone) {
        windows_standalone_requested = active;
    } else {
        windows_manual_requested = active;
    }

    bool desired = windows_manual_requested || windows_standalone_requested;

    if (!windows_output_faulted && desired != windows_last_queued_target) {
        if (windows_pending_count >= WINDOWS_PENDING_CAPACITY) {
            windows_output_faulted = true;
            overflow = true;
        } else {
            uint8_t tail = (windows_pending_head + windows_pending_count) %
                WINDOWS_PENDING_CAPACITY;
            windows_pending_targets[tail] = desired;
            windows_pending_count++;
            windows_last_queued_target = desired;
            enqueue = true;
        }
    }

    k_spin_unlock(&windows_lock, key);

    if (overflow) {
        LOG_ERR("Windows DPI transition queue overflow; controller faulted");
    } else if (enqueue) {
        windows_queue_reconcile();
    }
}

/*
 * Automatic mouse-layer activation is deliberately excluded by the caller.
 */
void behavior_manual_temporary_dpi_set(bool windows_host, bool active) {
    if (windows_host) {
        windows_set_request(false, active);
    } else {
        mac_set_request(false, active);
    }
}

static int temporary_dpi_binding_update(
    struct zmk_behavior_binding *binding, bool pressed) {

    const struct device *dev =
        zmk_behavior_get_binding(binding->behavior_dev);

    if (dev == NULL || dev->config == NULL) {
        LOG_ERR("Temporary-DPI behavior configuration unavailable");
        return -ENODEV;
    }

    const struct temporary_dpi_config *config = dev->config;

    if (config->windows_host) {
        windows_set_request(true, pressed);
    } else {
        mac_set_request(true, pressed);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

static int on_temporary_dpi_pressed(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    return temporary_dpi_binding_update(binding, true);
}

static int on_temporary_dpi_released(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    return temporary_dpi_binding_update(binding, false);
}

static const struct behavior_driver_api temporary_dpi_driver_api = {
    .binding_pressed = on_temporary_dpi_pressed,
    .binding_released = on_temporary_dpi_released,
};

static int temporary_dpi_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define TEMPORARY_DPI_INST(n) \
    static const struct temporary_dpi_config temporary_dpi_config_##n = { \
        .windows_host = DT_INST_PROP_OR(n, windows_host, 0), \
    }; \
    BEHAVIOR_DT_INST_DEFINE( \
        n, \
        temporary_dpi_init, \
        NULL, \
        NULL, \
        &temporary_dpi_config_##n, \
        POST_KERNEL, \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, \
        &temporary_dpi_driver_api \
    );

DT_INST_FOREACH_STATUS_OKAY(TEMPORARY_DPI_INST)
