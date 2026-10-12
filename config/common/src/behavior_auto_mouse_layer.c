#define DT_DRV_COMPAT zmk_behavior_auto_mouse_layer

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <raw_hid/events.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/layer_state_changed.h>
#include <zephyr/sys/util.h>
#include "behavior_mouse_layer.h"

#define PLOOPY_AUTO_MOUSE_LAYER 0x41
#define PLOOPY_AUTO_MOUSE_LAYER_VERSION 0x01
#define PLOOPY_CAPS_LOCK_INDICATOR BIT(1)

struct auto_mouse_layer_config {
    zmk_keymap_layer_id_t mac_base_layer;
    zmk_keymap_layer_id_t mac_mouse_layer;
    zmk_keymap_layer_id_t windows_base_layer;
    zmk_keymap_layer_id_t windows_mouse_layer;
};

#define AUTO_MOUSE_LAYER_CONFIG DT_PATH(behaviors, auto_mouse_layer_config)
static const struct auto_mouse_layer_config cfg = {
    .mac_base_layer = DT_PROP(AUTO_MOUSE_LAYER_CONFIG, mac_base_layer),
    .mac_mouse_layer = DT_PROP(AUTO_MOUSE_LAYER_CONFIG, mac_mouse_layer),
    .windows_base_layer = DT_PROP(AUTO_MOUSE_LAYER_CONFIG, windows_base_layer),
    .windows_mouse_layer = DT_PROP(AUTO_MOUSE_LAYER_CONFIG, windows_mouse_layer),
};

/* Each activation source has its own request. Never let one source release another's layer. */
static bool mac_auto_requested;
static bool mac_mouse_layer_hold_requested;
static bool mac_mouse_layer_latch_requested;
static bool windows_mouse_layer_hold_requested;
static bool windows_mouse_layer_latch_requested;
static bool windows_caps_known;
static bool windows_caps_on;
/* An explicit layer exit wins over a stale cached Caps-on state until Caps changes. */
static bool windows_caps_suppressed;
static bool mac_layer_owned;
static bool windows_layer_owned;
/* ZMK v0.3.0 dispatches layer-state listeners synchronously from the layer API.
 * This guard identifies callbacks caused by this controller, preventing them from
 * being misinterpreted as user-requested exits during reentrant event dispatch. */
static unsigned int controller_layer_change_depth;

static void reconcile_layer(zmk_keymap_layer_id_t layer, bool requested, bool *owned) {
    bool active = zmk_keymap_layer_active(layer);
    if (requested) {
        if (!active) {
            controller_layer_change_depth++;
            int rc = zmk_keymap_layer_activate(layer);
            controller_layer_change_depth--;
            if (rc == 0) {
                *owned = true;
            }
        }
        /* All configured layer-3/7 request paths are routed through this controller. */
        return;
    }
    if (*owned && active) {
        controller_layer_change_depth++;
        int rc = zmk_keymap_layer_deactivate(layer);
        controller_layer_change_depth--;
        if (rc == 0) {
            *owned = false;
        }
        return;
    }
    *owned = false;
}

static void reconcile(void) {
    bool windows_base = zmk_keymap_layer_active(cfg.windows_base_layer);
    bool mac_base = zmk_keymap_layer_active(cfg.mac_base_layer);
    bool mac_requested = mac_base && !windows_base &&
        (mac_auto_requested || mac_mouse_layer_hold_requested || mac_mouse_layer_latch_requested);
    bool windows_caps_requested = windows_base && windows_caps_known &&
        windows_caps_on && !windows_caps_suppressed;
    bool windows_requested = windows_base &&
        (windows_caps_requested || windows_mouse_layer_hold_requested || windows_mouse_layer_latch_requested);

    reconcile_layer(cfg.mac_mouse_layer, mac_requested, &mac_layer_owned);
    reconcile_layer(cfg.windows_mouse_layer, windows_requested, &windows_layer_owned);
}

static void auto_timeout(struct k_work *work) {
    ARG_UNUSED(work);
    mac_auto_requested = false;
    reconcile();
}
K_WORK_DELAYABLE_DEFINE(auto_timeout_work, auto_timeout);

void behavior_hold_mouse_layer_update(zmk_keymap_layer_id_t layer, bool pressed) {
    if (layer == cfg.mac_mouse_layer) {
        if (pressed) {
            /* The same N position exits a persistent latch; the transient hold keeps
             * the layer alive only until this key is released. */
            mac_mouse_layer_latch_requested = false;
            mac_mouse_layer_hold_requested = true;
            mac_auto_requested = false;
            k_work_cancel_delayable(&auto_timeout_work);
        } else {
            mac_mouse_layer_hold_requested = false;
        }
    } else if (layer == cfg.windows_mouse_layer) {
        if (pressed) {
            /* This behavior is bound on the Windows Mouse layer as its explicit exit
             * key. Suppress the still-on Caps request before reconciliation, otherwise
             * the cached Caps state would immediately keep the layer active. */
            windows_caps_suppressed = true;
            windows_mouse_layer_latch_requested = false;
            windows_mouse_layer_hold_requested = true;
        } else {
            windows_mouse_layer_hold_requested = false;
        }
    }
    reconcile();
}

/* Hold side of N_LatchMouseLayer*: persist until the mouse layer is explicitly exited. */
void behavior_latch_mouse_layer_set(zmk_keymap_layer_id_t layer) {
    if (layer == cfg.mac_mouse_layer) {
        mac_mouse_layer_latch_requested = true;
        mac_auto_requested = false;
        k_work_cancel_delayable(&auto_timeout_work);
    } else if (layer == cfg.windows_mouse_layer) {
        windows_mouse_layer_latch_requested = true;
    }
    reconcile();
}

static int hid_auto_mouse_layer_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (!event || event->length < 2 ||
        event->data[0] != PLOOPY_AUTO_MOUSE_LAYER ||
        event->data[1] != PLOOPY_AUTO_MOUSE_LAYER_VERSION) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* The timed Raw-HID auto path is Mac-only; Windows uses cached Caps state. */
    if (!zmk_keymap_layer_active(cfg.windows_base_layer) &&
        zmk_keymap_layer_active(cfg.mac_base_layer)) {
        mac_auto_requested = true;
        k_work_reschedule(&auto_timeout_work,
            K_MSEC(CONFIG_ZMK_BEHAVIOR_AUTO_MOUSE_LAYER_TIMEOUT_MS));
        reconcile();
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(hid_auto_mouse_layer, hid_auto_mouse_layer_listener);
ZMK_SUBSCRIPTION(hid_auto_mouse_layer, raw_hid_received_event);

static int layer_auto_mouse_layer_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *event = as_zmk_layer_state_changed(eh);
    if (!event) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* zmk_keymap_layer_activate/deactivate raises this event synchronously at the
     * pinned ZMK revision. Ignore our own mouse-layer notifications here: they are
     * implementation effects of reconcile(), not an independent exit request. */
    if (controller_layer_change_depth > 0 &&
        (event->layer == cfg.mac_mouse_layer ||
         event->layer == cfg.windows_mouse_layer)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (event->layer == cfg.windows_base_layer && event->state) {
        /* Clear stale Mac requests and initialize Caps state from the
         * currently selected host profile's cached HID indicators. */
        mac_auto_requested = false;
        mac_mouse_layer_hold_requested = false;
        mac_mouse_layer_latch_requested = false;
        k_work_cancel_delayable(&auto_timeout_work);
        windows_caps_on =
            (zmk_hid_indicators_get_current_profile() &
             PLOOPY_CAPS_LOCK_INDICATOR) != 0;
        windows_caps_known = true;
        windows_caps_suppressed = false;
    }
    if (event->layer == cfg.windows_base_layer && !event->state) {
        windows_mouse_layer_hold_requested = false;
        windows_mouse_layer_latch_requested = false;
        windows_caps_known = false;
        windows_caps_on = false;
        windows_caps_suppressed = false;
    }
    if (event->layer == cfg.mac_mouse_layer && !event->state) {
        /* An explicit deactivation clears stale Mac requests. */
        mac_auto_requested = false;
        mac_mouse_layer_hold_requested = false;
        mac_mouse_layer_latch_requested = false;
        k_work_cancel_delayable(&auto_timeout_work);
    }
    if (event->layer == cfg.windows_mouse_layer && !event->state) {
        /* The layer may have been exited explicitly while cached Caps was still on.
         * Suppress that stale request until a genuine Caps state change or OS re-entry. */
        if (zmk_keymap_layer_active(cfg.windows_base_layer)) {
            windows_caps_suppressed = true;
        }
        windows_mouse_layer_hold_requested = false;
        windows_mouse_layer_latch_requested = false;
    }

    reconcile();
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(layer_auto_mouse_layer, layer_auto_mouse_layer_listener);
ZMK_SUBSCRIPTION(layer_auto_mouse_layer, zmk_layer_state_changed);

static int led_auto_mouse_layer_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *event = as_zmk_hid_indicators_changed(eh);
    if (!event) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /* Cache only while Windows is selected. After a host-profile switch, require a
     * fresh indicator event rather than reusing potentially stale Caps state. */
    if (zmk_keymap_layer_active(cfg.windows_base_layer)) {
        bool next_caps_on = (event->indicators & PLOOPY_CAPS_LOCK_INDICATOR) != 0;
        if (!windows_caps_known || next_caps_on != windows_caps_on) {
            windows_caps_suppressed = false;
        }
        windows_caps_known = true;
        windows_caps_on = next_caps_on;
        reconcile();
    }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(led_auto_mouse_layer, led_auto_mouse_layer_listener);
ZMK_SUBSCRIPTION(led_auto_mouse_layer, zmk_hid_indicators_changed);
