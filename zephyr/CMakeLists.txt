# Auto Mouse
if(CONFIG_ZMK_BEHAVIOR_AUTO_MOUSE_LAYER)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/auto_mouse_layer.c
    )
endif()

# Drag Scroll
if(CONFIG_ZMK_BEHAVIOR_RAW_DRAG_SCROLL)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../config/common/src/behavior_raw_drag_scroll.c
    )
endif()
