#pragma once
#include <stdbool.h>
#include <zmk/keymap.h>
void behavior_manual_mouse_layer_update(zmk_keymap_layer_id_t layer, bool pressed);
void behavior_manual_mouse_layer_latch(zmk_keymap_layer_id_t layer);
