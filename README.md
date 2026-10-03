# ZMK-Firmware

Custom ZMK firmware configuration for the Corne and Totem split keyboards.

The repository provides separate firmware configurations for:

- **Corne-ZMK**
- **Totem-ZMK**

Both configurations share the same general logical layout and pointing features, while allowing each keyboard to have its own hardware-specific configuration.

## Features

### Mac and Windows layouts

The firmware provides separate Mac and Windows layer sets:

| OS | Base | Extend | Symbol | Mouse |
|---|---:|---:|---:|---:|
| macOS | 0 | 1 | 2 | 3 |
| Windows | 4 | 5 | 6 | 7 |

Additional layers are used for numeric, function, configuration, and reset functions.

### Auto Mouse Layer

The firmware supports automatic Mouse layer activation from Ploopy Nano-2 movement.

The implementation uses different notification paths depending on the operating system.

#### macOS

The Ploopy sends an Auto Mouse Layer notification through Raw HID.

The keyboard receives the notification and activates the configured Mac Mouse layer.

The Auto Mouse Layer timeout is configurable. A practical tuning range is approximately **300–700 ms**, depending on the desired balance between responsiveness and accidental layer activation.

#### Windows

Windows uses the Caps Lock LED state as the Auto Mouse Layer signal.

- Caps Lock ON → Windows Mouse layer ON
- Caps Lock OFF → Windows Mouse layer OFF

This path is state-based rather than timeout-based.

### DragScroll

The keyboard also supports the existing Ploopy Raw HID DragScroll functionality.

DragScroll is independent of Auto Mouse Layer.

The LED signals are kept separate:

- **Caps Lock** — Auto Mouse Layer
- **ScrollLock** — DragScroll

## Keyboard configurations

### Corne

The Corne configuration is located in:

    config/corne.conf
    config/corne.keymap
    config/corne.json

The firmware name is:

    Corne-ZMK

The Corne build configuration is:

    build/corne.yaml

### Totem

The Totem configuration is located in:

    config/totem.conf
    config/totem.keymap
    config/totem.json

The firmware name is:

    Totem-ZMK

The Totem build configuration is:

    build/totem.yaml

The Totem configuration accounts for its different physical key geometry. The outer bottom positions that do not exist on the physical keyboard are mapped to `&none`.

## Build

Firmware builds are handled by GitHub Actions using the ZMK v0.3.0 user configuration workflow.

The repository uses separate workflows for each keyboard:

    .github/workflows/build-corne.yml
    .github/workflows/build-totem.yml

The corresponding build matrices are:

    build/corne.yaml
    build/totem.yaml

The shared configuration is located under:

    config/common/

The ZMK west manifest is:

    config/west.yml

## Repository structure

    config/
    ├── common/
    ├── corne.conf
    ├── corne.json
    ├── corne.keymap
    ├── totem.conf
    ├── totem.json
    ├── totem.keymap
    └── west.yml

    build/
    ├── corne.yaml
    └── totem.yaml

    .github/workflows/
    ├── build-corne.yml
    └── build-totem.yml

The Corne and Totem configurations are intentionally kept independent so that changes specific to one keyboard do not unnecessarily affect the other.

## Firmware names

The firmware names reported by ZMK are:

    Corne-ZMK
    Totem-ZMK

These names identify the firmware configuration and are independent of the GitHub repository name.
