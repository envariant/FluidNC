# SignalHomingStepper Motor Driver

## Overview

The `SignalHomingStepper` is a specialized motor driver for FluidNC designed for motors that operate like standard steppers (using pulse and direction signals) but have their own internal homing mechanism and fault detection capabilities.

## Features

### 1. Signal-Based Homing
Unlike traditional stepper motors that home by moving until they hit a limit switch, the SignalHomingStepper initiates homing by sending a signal to the motor:

- **Homing Signal Pin**: A configurable output pin that pulses when homing is requested
- **Configurable Timeout**: The signal is held active for a specified duration
- **Automatic Completion**: After the timeout, the motor is automatically marked as homed
- **No Limit Switches Required**: The motor handles homing internally

### 2. Alarm Pin Monitoring
The driver can monitor an alarm/fault pin from the motor and trigger configurable actions:

- **Alarm Pin**: A configurable input pin that monitors the motor's fault status
- **Configurable Polarity**: Supports both active-high and active-low alarm signals
- **Multiple Actions**: Choose what happens when an alarm is detected:
  - `none`: Just log the alarm
  - `estop`: Trigger emergency stop (alarm state)
  - `pause`: Pause program execution (feed hold)
  - `macro0`/`macro1`/`macro2`/`macro3`: Execute a configured macro

## Configuration

### Basic Configuration Example

```yaml
x:
  motor0:
    signal_homing_stepper:
      # Standard stepper pins
      step_pin: I2SO.0
      direction_pin: I2SO.1
      disable_pin: I2SO.2

      # Signal homing configuration
      homing_pin: gpio.26
      homing_timeout_ms: 2000

      # Alarm pin configuration
      alarm_pin: gpio.27
      alarm_active_high: false
      alarm_action: estop
```

### Configuration Parameters

#### Standard Stepper Parameters
- `step_pin`: Pin for step pulses (required)
- `direction_pin`: Pin for direction signal (required)
- `disable_pin`: Pin to enable/disable the motor (optional)

#### Homing Parameters
- `homing_pin`: Pin that will be pulsed to initiate homing (optional, but required for homing)
- `homing_timeout_ms`: Duration to keep the homing signal active in milliseconds (default: 1000)

#### Alarm Parameters
- `alarm_pin`: Pin to monitor for motor alarms/faults (optional)
- `alarm_active_high`: Set to `true` if alarm is active-high, `false` if active-low (default: false)
- `alarm_action`: Action to take when alarm triggers (default: none)
  - `none`: Log the alarm but take no action
  - `estop`: Trigger emergency stop
  - `pause` or `hold`: Pause program execution
  - `macro0`, `macro1`, `macro2`, `macro3`: Execute the specified macro

## Use Cases

### Closed-Loop Stepper Motors
Many closed-loop stepper motors have:
- Built-in encoders for position tracking
- Internal homing routines that can be triggered by a signal
- Fault outputs for overcurrent, stall detection, etc.

The SignalHomingStepper is ideal for these motors.

### Example: Leadshine Closed-Loop Steppers
```yaml
motor0:
  signal_homing_stepper:
    step_pin: I2SO.0
    direction_pin: I2SO.1
    disable_pin: I2SO.2
    homing_pin: gpio.26        # Connected to motor's HOME signal input
    homing_timeout_ms: 3000    # Motor completes homing within 3 seconds
    alarm_pin: gpio.27         # Connected to motor's ALM output
    alarm_active_high: false   # ALM is active-low
    alarm_action: estop        # Stop everything on motor fault
```

### Example: Custom Motor Controller
```yaml
motor0:
  signal_homing_stepper:
    step_pin: I2SO.0
    direction_pin: I2SO.1
    homing_pin: gpio.26
    homing_timeout_ms: 5000
    alarm_pin: gpio.27
    alarm_active_high: false
    alarm_action: macro0       # Run custom error handling macro
```

With the macro:
```yaml
macros:
  macro0: |
    # Custom alarm handling
    G0 Z10           # Retract Z axis
    M5               # Stop spindle
    M117 Motor Fault Detected
```

## Homing Sequence

When a homing cycle (`$H` or `$HX`) is initiated:

1. **Signal Sent**: The homing pin is set high (or to the configured state)
2. **Wait Period**: The driver waits for `homing_timeout_ms` milliseconds
3. **Signal Released**: The homing pin is set low
4. **Completion**: The motor is marked as homed at the configured home position (`mpos_mm`)

The motor is expected to complete its internal homing routine during this time window.

## Alarm Handling

The alarm pin is continuously monitored. When an alarm condition is detected (based on `alarm_active_high` setting):

1. **Detection**: The alarm pin state matches the configured active state
2. **Logging**: A warning message is logged
3. **Action Execution**: The configured `alarm_action` is performed
4. **User Response**: The user must acknowledge the alarm (for `estop`) or the system resumes (for `pause`)

## Hardware Considerations

### Homing Pin
- **Output Type**: The homing pin is configured as a GPIO output
- **Signal Level**: Typically 3.3V logic (ESP32 native)
- **Duration**: Held high for `homing_timeout_ms`
- **Motor Interface**: Connect to your motor's homing trigger input
  - May require level shifting for 5V motors
  - May require opto-isolation depending on motor

### Alarm Pin
- **Input Type**: The alarm pin is configured as a GPIO input with pull-up
- **Signal Level**: Typically 3.3V logic (ESP32 native)
- **Polarity**: Configure `alarm_active_high` to match your motor's output
- **Motor Interface**: Connect to your motor's alarm/fault output
  - May require level shifting
  - May require opto-isolation
  - Consider adding RC filtering for noise immunity

### Example Circuit

```
Motor Homing Input ←─[Level Shift/Optocoupler]─ ESP32 GPIO (homing_pin)
Motor Alarm Output ─→[Level Shift/Optocoupler]─→ ESP32 GPIO (alarm_pin)
```

## Troubleshooting

### Homing Doesn't Complete
- **Check timeout**: Ensure `homing_timeout_ms` is long enough for your motor
- **Verify signal**: Use an oscilloscope to confirm the homing pin is pulsing
- **Check wiring**: Verify the homing pin is connected to the motor's trigger input
- **Motor settings**: Ensure the motor is configured to respond to the homing signal

### Alarm Triggers Unexpectedly
- **Check polarity**: Verify `alarm_active_high` matches your motor's output
- **Add filtering**: Electrical noise can cause false triggers - add RC filter
- **Check motor**: The motor may actually be faulting - check motor documentation

### Alarm Doesn't Trigger
- **Verify wiring**: Ensure alarm pin is properly connected
- **Test pin**: Manually short/open the alarm pin to verify detection
- **Check polarity**: Try toggling `alarm_active_high`

## Limitations

1. **No Position Feedback**: Unlike servo systems, the driver doesn't verify the motor actually reached home
2. **Blocking Homing**: The homing sequence blocks for the full timeout duration
3. **Single Alarm Pin**: Only one alarm pin per motor (cannot distinguish fault types)

## Advanced: Multiple Alarm Actions

You can combine alarm detection with macros for sophisticated error handling:

```yaml
motor0:
  signal_homing_stepper:
    # ... standard config ...
    alarm_action: macro0

macros:
  macro0: |
    # Advanced fault handling
    G0 Z10                    # Safe Z retract
    M5                        # Stop spindle
    M8 M9                     # Turn off coolant
    (MSG, Motor fault on X axis - Check motor driver)
    # Could also send email, trigger external alarm, etc.
```

## Comparison with Standard Steppers

| Feature | StandardStepper | SignalHomingStepper |
|---------|----------------|-------------------|
| Step/Dir control | ✓ | ✓ |
| Limit switches | Required for homing | Not used |
| Homing method | Move to limit | Signal-based |
| Alarm monitoring | Via separate pins | Integrated |
| Timeout-based homing | ✗ | ✓ |
| Custom alarm actions | ✗ | ✓ |

## See Also

- Example configuration: `example_configs/signal_homing_stepper_example.yaml`
- Source code: `FluidNC/src/Motors/SignalHomingStepper.h` and `.cpp`
- FluidNC Homing documentation
- Motor driver-specific documentation for your hardware

## Version History

- **v1.0** (2025): Initial implementation
  - Signal-based homing
  - Configurable alarm pin
  - Multiple alarm actions (estop, pause, macros)
