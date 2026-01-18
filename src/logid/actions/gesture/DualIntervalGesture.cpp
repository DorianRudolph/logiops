/*
 * Copyright 2019-2023 PixlOne
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <actions/gesture/DualIntervalGesture.h>
#include <Configuration.h>
#include <util/log.h>
#include <features/HapticFeedback.h>
#include <Device.h>
#include <util/task.h>
#include <InputDevice.h>
#include <cstdlib>

using namespace logid::actions;

const char* DualIntervalGesture::interface_name = "OnIntervalDual";

DualIntervalGesture::DualIntervalGesture(
        Device* device, config::DualIntervalGesture& config,
        const std::shared_ptr<ipcgull::node>& parent) :
        Gesture(device, parent, interface_name, {
                {
                        {"GetConfig", {this, &DualIntervalGesture::getConfig, {"interval", "threshold"}}},
                        {"SetInterval", {this, &DualIntervalGesture::setInterval, {"interval"}}},
                        {"SetThreshold", {this, &DualIntervalGesture::setThreshold, {"threshold"}}},
                        {"SetActionPositive", {this, &DualIntervalGesture::setActionPositive, {"type"}}},
                        {"SetActionNegative", {this, &DualIntervalGesture::setActionNegative, {"type"}}}
                },
                {},
                {}
        }),
        _axis(0),
        _last_trigger_axis(0),
        _interval_pass_count(0),
        _config(config),
        _hold_keys_pressed(false),
        _threshold_met(false),
        _positive_node(_node->make_child("positive")),
        _negative_node(_node->make_child("negative")) {
    if (config.action_positive) {
        try {
            _action_positive = Action::makeAction(
                    device, config.action_positive.value(), _positive_node);
        } catch (InvalidAction& e) {
            logPrintf(WARN, "Mapping positive interval gesture to invalid action");
        }
    }
    if (config.action_negative) {
        try {
            _action_negative = Action::makeAction(
                    device, config.action_negative.value(), _negative_node);
        } catch (InvalidAction& e) {
            logPrintf(WARN, "Mapping negative interval gesture to invalid action");
        }
    }

    // Parse hold_keys configuration
    if (_config.hold_keys.has_value()) {
        auto& hold_config = _config.hold_keys.value();
        if (std::holds_alternative<std::string>(hold_config)) {
            const auto& key = std::get<std::string>(hold_config);
            try {
                auto code = _device->virtualInput()->toKeyCode(key);
                _device->virtualInput()->registerKey(code);
                _hold_keys.emplace_back(code);
            } catch (InputDevice::InvalidEventCode& e) {
                logPrintf(WARN, "Invalid hold keycode %s, skipping.", key.c_str());
            }
        } else if (std::holds_alternative<uint>(hold_config)) {
            const auto& key = std::get<uint>(hold_config);
            _device->virtualInput()->registerKey(key);
            _hold_keys.emplace_back(key);
        } else if (std::holds_alternative<
                std::list<std::variant<uint, std::string>>>(hold_config)) {
            const auto& keys = std::get<
                    std::list<std::variant<uint, std::string>>>(hold_config);
            for (const auto& key: keys) {
                if (std::holds_alternative<std::string>(key)) {
                    const auto& key_str = std::get<std::string>(key);
                    try {
                        auto code = _device->virtualInput()->toKeyCode(key_str);
                        _device->virtualInput()->registerKey(code);
                        _hold_keys.emplace_back(code);
                    } catch (InputDevice::InvalidEventCode& e) {
                        logPrintf(WARN, "Invalid hold keycode %s, skipping.",
                                  key_str.c_str());
                    }
                } else if (std::holds_alternative<uint>(key)) {
                    auto& code = std::get<uint>(key);
                    _device->virtualInput()->registerKey(code);
                    _hold_keys.emplace_back(code);
                }
            }
        }
    }
}

void DualIntervalGesture::press(bool init_threshold) {
    std::shared_lock lock(_config_mutex);
    _axis = 0;
    _last_trigger_axis = 0;
    _interval_pass_count = 0;
    _hold_keys_pressed = false;
    const auto threshold =
            _config.threshold.value_or(defaults::gesture_threshold);
    _threshold_met = init_threshold || threshold == 0;
}

void DualIntervalGesture::release([[maybe_unused]] bool primary) {
    // Release the hold_keys only if they were pressed
    if (_hold_keys_pressed) {
        for (auto& key: _hold_keys)
            _device->virtualInput()->releaseKey(key);
        _hold_keys_pressed = false;
    }
}

void DualIntervalGesture::move(int16_t axis) {
    std::shared_lock lock(_config_mutex);
    if (!_config.interval.has_value())
        return;

    if (axis == 0)
        return;

    _axis += axis;
    const auto threshold =
            _config.threshold.value_or(defaults::gesture_threshold);
    if (!_threshold_met) {
        if (std::abs(_axis) < threshold)
            return;
        _threshold_met = true;
        _last_trigger_axis = _axis >= 0 ? threshold : -threshold;
    }

    // Press hold_keys when threshold is first crossed
    if (!_hold_keys_pressed && !_hold_keys.empty()) {
        for (auto& key: _hold_keys)
            _device->virtualInput()->pressKey(key);
        _hold_keys_pressed = true;
    }

    const auto interval = _config.interval.value();
    const int32_t delta = _axis - _last_trigger_axis;
    if (delta >= interval) {
        const int32_t step_count = delta / interval;
        if (_action_positive) {
            _action_positive->press();
            _action_positive->release();
        }
        _last_trigger_axis += step_count * interval;
    } else if (delta <= -interval) {
        const int32_t step_count = delta / interval;
        if (_action_negative) {
            _action_negative->press();
            _action_negative->release();
        }
        _last_trigger_axis += step_count * interval;
    } else {
        return;
    }

    // Play haptic asynchronously after action to avoid blocking
    // Only on the first interval unless haptic_every_interval is enabled
    if (_config.haptic_effect.has_value() &&
            (_interval_pass_count == 0 || _config.haptic_every_interval.value_or(false))) {
        auto effect = _config.haptic_effect.value();
        run_task([device = _device, effect] {
            try {
                auto haptic = device->getFeature<features::HapticFeedback>("hapticfeedback");
                if (haptic) {
                    haptic->playEffect(effect);
                }
            } catch (...) {
                // Haptic feedback not supported, ignore
            }
        });
    }
    ++_interval_pass_count;
}

bool DualIntervalGesture::wheelCompatibility() const {
    return true;
}

bool DualIntervalGesture::metThreshold() const {
    std::shared_lock lock(_config_mutex);
    return _threshold_met;
}

std::tuple<int, int> DualIntervalGesture::getConfig() const {
    std::shared_lock lock(_config_mutex);
    return {_config.interval.value_or(0), _config.threshold.value_or(0)};
}

void DualIntervalGesture::setInterval(int interval) {
    std::unique_lock lock(_config_mutex);
    if (interval == 0)
        _config.interval.reset();
    else
        _config.interval = interval;
}

void DualIntervalGesture::setThreshold(int threshold) {
    std::unique_lock lock(_config_mutex);
    if (threshold == 0)
        _config.threshold.reset();
    else
        _config.threshold = threshold;
}

void DualIntervalGesture::setActionPositive(const std::string& type) {
    std::unique_lock lock(_config_mutex);
    _action_positive.reset();
    _action_positive = Action::makeAction(
            _device, type, _config.action_positive, _positive_node);
}

void DualIntervalGesture::setActionNegative(const std::string& type) {
    std::unique_lock lock(_config_mutex);
    _action_negative.reset();
    _action_negative = Action::makeAction(
            _device, type, _config.action_negative, _negative_node);
}
