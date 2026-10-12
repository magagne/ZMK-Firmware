# Auto Mouse
if(CONFIG_ZMK_BEHAVIOR_AUTO_MOUSE_LAYER)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_auto_mouse_layer.c
    )
endif()

# Manual Mouse Layer
if(CONFIG_ZMK_BEHAVIOR_MANUAL_MOUSE_LAYER)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_manual_mouse_layer.c
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_manual_mouse_layer_latch.c
    )
endif()

# Drag Scroll
if(CONFIG_ZMK_BEHAVIOR_RAW_DRAG_SCROLL)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_raw_drag_scroll.c
    )
endif()

# Temporary DPI
if(CONFIG_ZMK_BEHAVIOR_RAW_TEMPORARY_DPI)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_raw_temporary_dpi.c
    )
endif()
