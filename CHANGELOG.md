# Changelog

All notable changes to this fork will be documented in this file.

## [Unreleased]

### Added
- **Haptic Feedback Support for Gestures**: Added optional `haptic_effect` parameter to gesture actions for devices with haptic feedback support (e.g., MX Master 4)
  - AxisGesture: Play haptic feedback on first threshold crossing (src/logid/actions/gesture/AxisGesture.cpp)
  - IntervalGesture: Play haptic feedback on first interval trigger (src/logid/actions/gesture/IntervalGesture.cpp)
  - ReleaseGesture: Play haptic feedback when gesture is released (src/logid/actions/gesture/ReleaseGesture.cpp)
  - ThresholdGesture: Play haptic feedback when threshold is crossed (src/logid/actions/gesture/ThresholdGesture.cpp)
- Haptic feedback is executed asynchronously to avoid blocking gesture actions
- Configuration schema updated to support `haptic_effect` parameter in all gesture types (src/logid/config/schema.h)

### Fixed
- **CycleDPI Logic Error**: Fixed incorrect condition in CycleDPI that was checking if DPI list was empty instead of not empty (src/logid/actions/CycleDPI.cpp:68)
  - Changed `_config.dpis.value().empty()` to `!_config.dpis.value().empty()`
  - This fix allows DPI cycling to work correctly when DPI list is configured

### Technical Details
- Haptic feedback implementation gracefully handles devices without haptic support through exception catching
- Uses `run_task` for asynchronous haptic effect execution
- Added includes for `features/HapticFeedback.h` and `util/task.h` in gesture files

## Configuration Example

To use haptic feedback in your gestures, add the `haptic_effect` parameter:

```
gestures: {
    threshold: 30,
    mode: "Axis",
    axis_multiplier: 2.0,
    haptic_effect: 1  // Optional: play haptic effect on gesture trigger
};
```

---

Based on commit: 4319de5 (Add MX Master 4 haptic feedback feature)
