# Fork Changes

This fork of logiops adds enhancements and bug fixes to the original project.

## Why This Fork?

This fork extends logiops with:
1. Haptic feedback support for modern Logitech devices
2. Critical bug fixes for existing features

## Key Differences from Upstream

### 1. Haptic Feedback Feature

Added support for haptic feedback on gesture actions for devices like the MX Master 4 that support the HapticFeedback feature.

**Usage:**
Add the optional `haptic_effect` parameter to any gesture configuration:

```cfg
buttons: (
    {
        cid: 0xc4;
        action: {
            type: "Gestures";
            gestures: (
                {
                    direction: "Up";
                    mode: "OnThreshold";
                    threshold: 50;
                    haptic_effect: 1;  // Play haptic feedback when triggered
                    action: {
                        type: "Keypress";
                        keys: ["KEY_VOLUMEUP"];
                    };
                }
            );
        };
    }
);
```

**Supported Gestures:**
- `AxisGesture` - Haptic plays on first threshold crossing
- `IntervalGesture` - Haptic plays on first interval
- `ReleaseGesture` - Haptic plays when gesture is released
- `ThresholdGesture` - Haptic plays when threshold is crossed

**Implementation Details:**
- Haptic feedback executes asynchronously to prevent blocking
- Gracefully degrades on devices without haptic support
- No configuration changes required for devices without haptic support

### 2. CycleDPI Bug Fix

Fixed a critical logic error in DPI cycling where the condition was inverted:
- **Before:** Would only cycle if DPI list was empty (never worked)
- **After:** Cycles through DPI list when properly configured

**File:** `src/logid/actions/CycleDPI.cpp:68`

## Installation

Follow the same build instructions as the upstream project:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install
```

## Contributing

This fork maintains compatibility with the upstream project. When contributing:
- Ensure changes don't break existing configurations
- Add appropriate documentation for new features
- Test with devices that both support and don't support new features

## Upstream Compatibility

This fork is based on commit `628ab93` from [PixlOne/logiops](https://github.com/PixlOne/logiops) and includes all upstream features up to that point.

## License

This project maintains the same license as the upstream logiops project.
