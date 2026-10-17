#pragma once
#include <stdbool.h>
#include <zmk/keymap.h>
void behavior_hold_mouse_layer_update(zmk_keymap_layer_id_t layer, bool pressed);
void behavior_latch_mouse_layer_set(zmk_keymap_layer_id_t layer);
void behavior_unlatch_mouse_layer(zmk_keymap_layer_id_t mouse_layer, zmk_keymap_layer_id_t base_layer);
