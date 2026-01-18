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
#ifndef LOGID_ACTION_DUALINTERVALGESTURE_H
#define LOGID_ACTION_DUALINTERVALGESTURE_H

#include <actions/gesture/Gesture.h>

namespace logid::actions {
    class DualIntervalGesture : public Gesture {
    public:
        static const char* interface_name;

        DualIntervalGesture(Device* device, config::DualIntervalGesture& config,
                            const std::shared_ptr<ipcgull::node>& parent);

        void press(bool init_threshold) final;

        void release(bool primary) final;

        void move(int16_t axis) final;

        [[nodiscard]] bool wheelCompatibility() const final;

        [[nodiscard]] bool metThreshold() const final;

        [[nodiscard]] std::tuple<int, int> getConfig() const;

        void setInterval(int interval);

        void setThreshold(int threshold);

        void setActionPositive(const std::string& type);

        void setActionNegative(const std::string& type);

    private:
        int32_t _axis;
        int32_t _last_trigger_axis;
        int32_t _interval_pass_count;
        std::shared_ptr<Action> _action_positive;
        std::shared_ptr<Action> _action_negative;
        config::DualIntervalGesture& _config;
        std::list<uint> _hold_keys;
        bool _hold_keys_pressed;
        bool _threshold_met;
        std::shared_ptr<ipcgull::node> _positive_node;
        std::shared_ptr<ipcgull::node> _negative_node;
    };
}

#endif //LOGID_ACTION_DUALINTERVALGESTURE_H
