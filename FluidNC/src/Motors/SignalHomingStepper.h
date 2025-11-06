// Copyright (c) 2025
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "StandardStepper.h"
#include "../Machine/EventPin.h"
#include "../Macro.h"

namespace MotorDrivers {
    // Forward declaration for the alarm pin
    class SignalHomingStepper;

    // AlarmPin monitors an input pin and triggers configurable actions
    class MotorAlarmPin : public InputPin {
    private:
        SignalHomingStepper* _parent;

    public:
        MotorAlarmPin(const char* legend, SignalHomingStepper* parent);
        void trigger(bool active) override;
        ~MotorAlarmPin() {}
    };

    /**
     * SignalHomingStepper - A stepper motor driver with special homing and alarm features
     *
     * This motor operates like a standard stepper with pulse/dir signals, but handles
     * homing differently:
     * - Homing sends a signal via a configurable pin
     * - After a configurable timeout, the motor is automatically marked as homed
     *
     * Two homing modes are supported:
     * - Hold: Signal is held active for the entire duration until homing completes
     * - Pulse: Signal is pulsed briefly, then released while waiting for motor to home
     *
     * It also supports an alarm pin that can trigger various actions:
     * - e-stop: Trigger an alarm and halt the system
     * - pause: Pause program execution (feed hold)
     * - macro0-3: Execute a configured macro
     */
    class SignalHomingStepper : public StandardStepper {
    public:
        enum class HomingMode {
            Hold,   // Hold signal active for entire homing duration
            Pulse   // Pulse signal briefly, then wait for motor to home
        };

        enum class AlarmAction {
            None,
            EStop,
            Pause,
            Macro0,
            Macro1,
            Macro2,
            Macro3
        };

        SignalHomingStepper(const char* name) : StandardStepper(name), _alarmPin("alarm_pin", this) {}

        // Overrides for inherited methods
        void init() override;
        bool set_homing_mode(bool isHoming) override;

        // Alarm handling
        void handleAlarmTrigger(bool active);
        AlarmAction alarmAction() const { return _alarmAction; }

    protected:
        void config_message() override;
        void validate() override;

        void group(Configuration::HandlerBase& handler) override;

    private:
        Pin             _homingPin;                      // Pin to signal when homing
        HomingMode      _homingMode = HomingMode::Hold;  // Homing signal mode
        uint32_t        _homingSignalMs = 100;           // Duration of homing signal pulse/hold
        uint32_t        _homingSettleMs = 2000;          // Time to wait for motor to complete homing
        MotorAlarmPin   _alarmPin;                       // Pin to monitor for alarm conditions
        bool            _alarmActiveHigh = false;        // Alarm pin polarity
        AlarmAction     _alarmAction = AlarmAction::None;

        // State tracking
        bool            _isHoming = false;

        // Helper methods
        void            startHomingSequence();
        void            finishHomingSequence();
        static HomingMode parseHomingMode(const char* str);
        static const char* homingModeToString(HomingMode mode);
        static AlarmAction parseAlarmAction(const char* str);
        static const char* alarmActionToString(AlarmAction action);
    };
}
