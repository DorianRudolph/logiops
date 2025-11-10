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
#include <actions/gesture/IntervalGesture.h>
#include <Configuration.h>
#include <util/log.h>
#include <features/HapticFeedback.h>
#include <Device.h>
#include <util/task.h>
#include <InputDevice.h>

using namespace logid::actions;

const char* IntervalGesture::interface_name = "OnInterval";

IntervalGesture::IntervalGesture(
        Device* device, config::IntervalGesture& config,
        const std::shared_ptr<ipcgull::node>& parent) :
        Gesture(device, parent, interface_name, {
                {
                        {"GetConfig", {this, &IntervalGesture::getConfig, {"interval", "threshold"}}},
                        {"SetInterval", {this, &IntervalGesture::setInterval, {"interval"}}},
                        {"SetThreshold", {this, &IntervalGesture::setThreshold, {"interval"}}},
                        {"SetAction", {this, &IntervalGesture::setAction, {"type"}}}
                },
                {},
                {}
        }),
        _axis(0), _interval_pass_count(0), _config(config), _hold_keys_pressed(false) {
    if (config.action) {
        try {
            _action = Action::makeAction(device, config.action.value(), _node);
        } catch (InvalidAction& e) {
            logPrintf(WARN, "Mapping gesture to invalid action");
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

void IntervalGesture::press(bool init_threshold) {
    std::shared_lock lock(_config_mutex);
    if (init_threshold) {
        _axis = (int32_t) _config.threshold.value_or(defaults::gesture_threshold);
    } else {
        _axis = 0;
    }
    _interval_pass_count = 0;
    _hold_keys_pressed = false;
}

void IntervalGesture::release([[maybe_unused]] bool primary) {
    // Release the hold_keys only if they were pressed
    if (_hold_keys_pressed) {
        for (auto& key: _hold_keys)
            _device->virtualInput()->releaseKey(key);
        _hold_keys_pressed = false;
    }
}

void IntervalGesture::move(int16_t axis) {
    std::shared_lock lock(_config_mutex);
    if (!_config.interval.has_value())
        return;

    const auto threshold =
            _config.threshold.value_or(defaults::gesture_threshold);
    _axis += axis;
    if (_axis < threshold)
        return;

    // Press hold_keys when threshold is first crossed
    if (!_hold_keys_pressed && !_hold_keys.empty()) {
        for (auto& key: _hold_keys)
            _device->virtualInput()->pressKey(key);
        _hold_keys_pressed = true;
    }

    int32_t new_interval_count = (_axis - threshold) / _config.interval.value();
    if (new_interval_count > _interval_pass_count) {
        if (_action) {
            _action->press();
            _action->release();
        }
        // Play haptic asynchronously after action to avoid blocking
        // Only on the first interval to avoid interrupting continuous actions
        if (_interval_pass_count == 0 && _config.haptic_effect.has_value()) {
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
    }
    _interval_pass_count = new_interval_count;
}

bool IntervalGesture::wheelCompatibility() const {
    return true;
}

bool IntervalGesture::metThreshold() const {
    std::shared_lock lock(_config_mutex);
    return _axis >= _config.threshold.value_or(defaults::gesture_threshold);
}

std::tuple<int, int> IntervalGesture::getConfig() const {
    std::shared_lock lock(_config_mutex);
    return {_config.interval.value_or(0), _config.threshold.value_or(0)};
}

void IntervalGesture::setInterval(int interval) {
    std::unique_lock lock(_config_mutex);
    if (interval == 0)
        _config.interval.reset();
    else
        _config.interval = interval;
}

void IntervalGesture::setThreshold(int threshold) {
    std::unique_lock lock(_config_mutex);
    if (threshold == 0)
        _config.threshold.reset();
    else
        _config.threshold = threshold;
}

void IntervalGesture::setAction(const std::string& type) {
    std::unique_lock lock(_config_mutex);
    _action.reset();
    _action = Action::makeAction(_device, type, _config.action, _node);
}
