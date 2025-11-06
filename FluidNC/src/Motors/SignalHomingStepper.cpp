// Copyright (c) 2025
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "SignalHomingStepper.h"
#include "../Machine/MachineConfig.h"
#include "../Machine/Macros.h"
#include "../Protocol.h"
#include "../Report.h"
#include "../NutsBolts.h"

#include <cstring>

using namespace Machine;

namespace MotorDrivers {

    // ===== MotorAlarmPin Implementation =====

    MotorAlarmPin::MotorAlarmPin(const char* legend, SignalHomingStepper* parent)
        : InputPin(legend), _parent(parent) {}

    void MotorAlarmPin::trigger(bool active) {
        InputPin::trigger(active);
        if (_parent) {
            _parent->handleAlarmTrigger(active);
        }
    }

    // ===== SignalHomingStepper Implementation =====

    void SignalHomingStepper::init() {
        // Initialize base stepper pins
        StandardStepper::init();

        // Initialize homing pin if configured
        if (_homingPin.defined()) {
            _homingPin.setAttr(Pin::Attr::Output);
            _homingPin.synchronousWrite(false);  // Start with signal off
        }

        // Initialize alarm pin if configured
        if (_alarmPin.defined()) {
            _alarmPin.init();
        }
    }

    bool SignalHomingStepper::set_homing_mode(bool isHoming) {
        if (isHoming) {
            startHomingSequence();
            // Return false to indicate we handle homing independently
            // This tells the homing system to skip limit-based homing for this motor
            return false;
        } else {
            finishHomingSequence();
            return false;
        }
    }

    void SignalHomingStepper::startHomingSequence() {
        if (!_homingPin.defined()) {
            log_warn(axisName() << " SignalHomingStepper: Homing pin not configured");
            return;
        }

        _isHoming = true;

        if (_homingMode == HomingMode::Hold) {
            // Mode A: Hold signal active for entire homing duration
            _homingPin.synchronousWrite(true);
            log_info(axisName() << " SignalHomingStepper: Homing signal active (hold mode, "
                     << _homingSignalMs << "ms)");

            delay_ms(_homingSignalMs);

            _homingPin.synchronousWrite(false);
            log_info(axisName() << " SignalHomingStepper: Homing complete");

        } else {
            // Mode B: Pulse signal briefly, then wait for motor to complete homing
            _homingPin.synchronousWrite(true);
            log_info(axisName() << " SignalHomingStepper: Homing pulse sent ("
                     << _homingSignalMs << "ms pulse)");

            delay_ms(_homingSignalMs);

            _homingPin.synchronousWrite(false);
            log_info(axisName() << " SignalHomingStepper: Waiting for motor to home ("
                     << _homingSettleMs << "ms)");

            delay_ms(_homingSettleMs);

            log_info(axisName() << " SignalHomingStepper: Homing complete (total: "
                     << (_homingSignalMs + _homingSettleMs) << "ms)");
        }
    }

    void SignalHomingStepper::finishHomingSequence() {
        _isHoming = false;

        // Ensure homing signal is off
        if (_homingPin.defined()) {
            _homingPin.synchronousWrite(false);
        }
    }

    void SignalHomingStepper::handleAlarmTrigger(bool active) {
        // Check if the alarm condition matches the configured polarity
        bool alarmCondition = _alarmActiveHigh ? active : !active;

        if (!alarmCondition) {
            return;  // Alarm not active
        }

        log_warn(axisName() << " SignalHomingStepper: Alarm triggered!");

        // Execute the configured action
        switch (_alarmAction) {
            case AlarmAction::EStop:
                log_error(axisName() << " SignalHomingStepper: Triggering E-Stop");
                send_alarm(ExecAlarm::HardLimit);
                break;

            case AlarmAction::Pause:
                log_info(axisName() << " SignalHomingStepper: Triggering Feed Hold");
                protocol_send_event(&feedHoldEvent);
                break;

            case AlarmAction::Macro0:
                log_info(axisName() << " SignalHomingStepper: Executing Macro 0");
                protocol_send_event(&macro0Event);
                break;

            case AlarmAction::Macro1:
                log_info(axisName() << " SignalHomingStepper: Executing Macro 1");
                protocol_send_event(&macro1Event);
                break;

            case AlarmAction::Macro2:
                log_info(axisName() << " SignalHomingStepper: Executing Macro 2");
                protocol_send_event(&macro2Event);
                break;

            case AlarmAction::Macro3:
                log_info(axisName() << " SignalHomingStepper: Executing Macro 3");
                protocol_send_event(&macro3Event);
                break;

            case AlarmAction::None:
            default:
                log_debug(axisName() << " SignalHomingStepper: Alarm detected but no action configured");
                break;
        }
    }

    void SignalHomingStepper::config_message() {
        std::string homingInfo;
        if (_homingMode == HomingMode::Hold) {
            homingInfo = std::to_string(_homingSignalMs) + "ms hold";
        } else {
            homingInfo = std::to_string(_homingSignalMs) + "ms pulse, " +
                        std::to_string(_homingSettleMs) + "ms settle";
        }

        log_info("    " << name() << " Step:" << _step_pin.name()
                 << " Dir:" << _dir_pin.name()
                 << " Disable:" << _disable_pin.name()
                 << " Homing:" << _homingPin.name() << "(" << homingModeToString(_homingMode) << ": " << homingInfo << ")"
                 << " Alarm:" << _alarmPin.name() << "(" << alarmActionToString(_alarmAction) << ")");
    }

    void SignalHomingStepper::validate() {
        StandardStepper::validate();

        if (_alarmAction != AlarmAction::None && !_alarmPin.defined()) {
            log_warn(axisName() << " SignalHomingStepper: Alarm action configured but alarm pin not defined");
        }
    }

    void SignalHomingStepper::group(Configuration::HandlerBase& handler) {
        // Include base stepper configuration
        handler.item("step_pin", _step_pin);
        handler.item("direction_pin", _dir_pin);
        handler.item("disable_pin", _disable_pin);

        // Add signal homing configuration
        handler.item("homing_pin", _homingPin);
        handler.section("homing_mode", _homingMode, parseHomingMode, homingModeToString);
        handler.item("homing_signal_ms", _homingSignalMs);
        handler.item("homing_settle_ms", _homingSettleMs);

        // Add alarm configuration
        handler.item("alarm_pin", _alarmPin);
        handler.item("alarm_active_high", _alarmActiveHigh);

        // Alarm action as a string
        handler.section("alarm_action", _alarmAction, parseAlarmAction, alarmActionToString);
    }

    SignalHomingStepper::HomingMode SignalHomingStepper::parseHomingMode(const char* str) {
        if (!str) {
            return HomingMode::Hold;
        }

        if (strcasecmp(str, "hold") == 0) {
            return HomingMode::Hold;
        } else if (strcasecmp(str, "pulse") == 0) {
            return HomingMode::Pulse;
        }

        log_warn("Unknown homing mode: " << str << ", using 'hold'");
        return HomingMode::Hold;
    }

    const char* SignalHomingStepper::homingModeToString(HomingMode mode) {
        switch (mode) {
            case HomingMode::Hold:
                return "hold";
            case HomingMode::Pulse:
                return "pulse";
            default:
                return "hold";
        }
    }

    SignalHomingStepper::AlarmAction SignalHomingStepper::parseAlarmAction(const char* str) {
        if (!str) {
            return AlarmAction::None;
        }

        if (strcasecmp(str, "estop") == 0 || strcasecmp(str, "e-stop") == 0) {
            return AlarmAction::EStop;
        } else if (strcasecmp(str, "pause") == 0 || strcasecmp(str, "hold") == 0) {
            return AlarmAction::Pause;
        } else if (strcasecmp(str, "macro0") == 0) {
            return AlarmAction::Macro0;
        } else if (strcasecmp(str, "macro1") == 0) {
            return AlarmAction::Macro1;
        } else if (strcasecmp(str, "macro2") == 0) {
            return AlarmAction::Macro2;
        } else if (strcasecmp(str, "macro3") == 0) {
            return AlarmAction::Macro3;
        } else if (strcasecmp(str, "none") == 0) {
            return AlarmAction::None;
        }

        log_warn("Unknown alarm action: " << str << ", using 'none'");
        return AlarmAction::None;
    }

    const char* SignalHomingStepper::alarmActionToString(AlarmAction action) {
        switch (action) {
            case AlarmAction::EStop:
                return "estop";
            case AlarmAction::Pause:
                return "pause";
            case AlarmAction::Macro0:
                return "macro0";
            case AlarmAction::Macro1:
                return "macro1";
            case AlarmAction::Macro2:
                return "macro2";
            case AlarmAction::Macro3:
                return "macro3";
            case AlarmAction::None:
            default:
                return "none";
        }
    }

    // Configuration registration
    namespace {
        MotorFactory::InstanceBuilder<SignalHomingStepper> registration("signal_homing_stepper");
    }
}
