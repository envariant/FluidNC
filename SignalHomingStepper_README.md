# SignalHomingStepper Motor Driver

## Overview

The `SignalHomingStepper` is a specialized motor driver for FluidNC designed for motors that operate like standard steppers (using pulse and direction signals) but have their own internal homing mechanism and fault detection capabilities.

## Features

### 1. Signal-Based Homing
Unlike traditional stepper motors that home by moving until they hit a limit switch, the SignalHomingStepper initiates homing by sending a signal to the motor:

- **Homing Signal Pin**: A configurable output pin that signals when homing is requested
- **Two Homing Modes**:
  - **Hold Mode**: Signal stays active for the entire homing duration
  - **Pulse Mode**: Signal pulses briefly, then waits for motor to complete homing
- **Configurable Durations**: Separate control of signal and settle times
- **Event-Driven Completion (Optional)**: Configure a homing complete pin to detect when the motor finishes homing instead of using timeouts
- **Automatic Completion**: After the configured time(s) or complete signal, the motor is automatically marked as homed
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

### Basic Configuration Example - Hold Mode

```yaml
x:
  motor0:
    signal_homing_stepper:
      # Standard stepper pins
      step_pin: I2SO.0
      direction_pin: I2SO.1
      disable_pin: I2SO.2

      # Signal homing configuration - Hold Mode
      homing_pin: gpio.26
      homing_mode: hold
      homing_signal_ms: 2000      # Signal stays active for 2 seconds
      homing_settle_ms: 0         # Not used in hold mode

      # Alarm pin configuration
      alarm_pin: gpio.27
      alarm_active_high: false
      alarm_action: estop
```

### Basic Configuration Example - Pulse Mode with Complete Detection

```yaml
y:
  motor0:
    signal_homing_stepper:
      # Standard stepper pins
      step_pin: I2SO.3
      direction_pin: I2SO.4
      disable_pin: I2SO.5

      # Signal homing configuration - Pulse Mode
      homing_pin: gpio.32
      homing_mode: pulse
      homing_signal_ms: 100       # Brief 100ms pulse

      # Option 1: Event-driven (wait for complete signal)
      homing_complete_pin: gpio.35
      homing_complete_active_high: true
      homing_max_wait_ms: 8000    # Safety timeout

      # Option 2: Time-based (if complete pin not configured)
      homing_settle_ms: 3000      # Wait 3 seconds for motor to home

      # Alarm pin configuration
      alarm_pin: gpio.33
      alarm_active_high: false
      alarm_action: pause
```

### Configuration Parameters

#### Standard Stepper Parameters
- `step_pin`: Pin for step pulses (required)
- `direction_pin`: Pin for direction signal (required)
- `disable_pin`: Pin to enable/disable the motor (optional)

#### Homing Parameters
- `homing_pin`: Pin that will be set high to initiate homing (optional, but required for homing)
- `homing_mode`: Homing signal mode (default: hold)
  - `hold`: Signal stays active for entire homing duration
  - `pulse`: Signal pulses briefly, then releases while motor homes
- `homing_signal_ms`: Duration of homing signal in milliseconds (default: 100)
  - In **hold mode**: How long the signal stays active
  - In **pulse mode**: How long the pulse lasts
- `homing_settle_ms`: Time to wait for motor to complete homing in milliseconds (default: 2000)
  - In **hold mode**: Not used (can be left at 0)
  - In **pulse mode**: How long to wait after pulse before marking as homed (only used if `homing_complete_pin` is not configured)

#### Homing Complete Detection (Optional)
- `homing_complete_pin`: Pin to monitor for homing completion signal (optional)
  - When configured in pulse mode, driver waits for this pin instead of using timeout
  - Motor sets this pin when homing is complete
  - Falls back to `homing_settle_ms` timeout if pin not configured
- `homing_complete_active_high`: Set to `true` if complete signal is active-high, `false` if active-low (default: true)
- `homing_max_wait_ms`: Maximum time to wait for complete signal before timeout (default: 10000)
  - Safety feature to prevent infinite waiting
  - If exceeded, assumes homing is complete anyway

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

### Example: Leadshine Closed-Loop Steppers (Pulse Mode with Complete Pin)
```yaml
motor0:
  signal_homing_stepper:
    step_pin: I2SO.0
    direction_pin: I2SO.1
    disable_pin: I2SO.2
    homing_pin: gpio.26            # Connected to motor's HOME signal input
    homing_mode: pulse             # Motor triggers on pulse
    homing_signal_ms: 50           # 50ms pulse

    # Event-driven completion
    homing_complete_pin: gpio.36   # Connected to motor's HOMING_DONE output
    homing_complete_active_high: true
    homing_max_wait_ms: 10000      # 10 second safety timeout

    # Fallback if motor doesn't signal completion
    homing_settle_ms: 5000

    alarm_pin: gpio.27             # Connected to motor's ALM output
    alarm_active_high: false       # ALM is active-low
    alarm_action: estop            # Stop everything on motor fault
```

### Example: Custom Motor Controller (Hold Mode)
```yaml
motor0:
  signal_homing_stepper:
    step_pin: I2SO.0
    direction_pin: I2SO.1
    homing_pin: gpio.26
    homing_mode: hold          # Motor needs continuous signal
    homing_signal_ms: 4000     # Keep signal active for 4 seconds
    homing_settle_ms: 0        # Not used in hold mode
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

### Hold Mode

When a homing cycle (`$H` or `$HX`) is initiated with hold mode:

1. **Signal Active**: The homing pin is set high
2. **Hold Period**: The signal stays active for `homing_signal_ms` milliseconds
3. **Signal Released**: The homing pin is set low
4. **Completion**: The motor is marked as homed at the configured home position (`mpos_mm`)

The motor is expected to complete its internal homing routine while the signal is held active.

**Use case**: Motors that need a continuous "enable homing" signal during the entire homing process.

### Pulse Mode

When a homing cycle (`$H` or `$HX`) is initiated with pulse mode:

1. **Signal Pulse**: The homing pin is set high
2. **Pulse Duration**: The signal stays active for `homing_signal_ms` milliseconds
3. **Signal Released**: The homing pin is set low
4. **Wait for Completion**:
   - **Option A (Event-Driven)**: If `homing_complete_pin` is configured:
     - Poll the complete pin every 10ms
     - When pin indicates completion, proceed immediately
     - If `homing_max_wait_ms` timeout is reached, assume completion
   - **Option B (Time-Based)**: If `homing_complete_pin` is NOT configured:
     - Wait for `homing_settle_ms` milliseconds
5. **Completion**: The motor is marked as homed at the configured home position (`mpos_mm`)

The motor starts homing when the pulse is received and completes either when signaled or after timeout.

**Use case**: Motors that start homing on a trigger pulse and complete autonomously (typical for closed-loop steppers).

**Advantage of complete pin**: More responsive - homing finishes as soon as motor completes, rather than waiting for worst-case timeout.

### Timing Comparison

| Mode | Signal Duration | Wait After Signal | Total Time |
|------|----------------|-------------------|------------|
| Hold | `homing_signal_ms` | 0 | `homing_signal_ms` |
| Pulse (time-based) | `homing_signal_ms` | `homing_settle_ms` | `homing_signal_ms + homing_settle_ms` |
| Pulse (event-driven) | `homing_signal_ms` | Until complete pin or `homing_max_wait_ms` | `homing_signal_ms` + actual homing time |

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
- **Duration**: Held high for configured duration (mode dependent)
- **Motor Interface**: Connect to your motor's homing trigger input
  - May require level shifting for 5V motors
  - May require opto-isolation depending on motor

### Homing Complete Pin (Optional)
- **Input Type**: The homing complete pin is configured as a GPIO input
- **Signal Level**: Typically 3.3V logic (ESP32 native)
- **Polarity**: Configure `homing_complete_active_high` to match your motor's output
- **Motor Interface**: Connect to your motor's "homing done" or "ready" output
  - May require level shifting
  - May require opto-isolation
  - Should be stable (not pulsed) - driver polls this pin
  - Consider adding RC filtering for noise immunity

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
Motor Homing Input    ←─[Level Shift/Optocoupler]─ ESP32 GPIO (homing_pin)
Motor Complete Output ─→[Level Shift/Optocoupler]─→ ESP32 GPIO (homing_complete_pin)
Motor Alarm Output    ─→[Level Shift/Optocoupler]─→ ESP32 GPIO (alarm_pin)
```

## Troubleshooting

### Homing Doesn't Complete
- **Check mode**: Verify you're using the correct `homing_mode` (hold vs pulse)
- **Check durations**:
  - For hold mode: Ensure `homing_signal_ms` is long enough
  - For pulse mode: Ensure `homing_signal_ms` pulse is detected and `homing_settle_ms` allows enough time
- **Check complete pin** (if using):
  - Verify pin is actually connected
  - Check polarity with `homing_complete_active_high`
  - Use oscilloscope to confirm motor sets the pin when done
  - Check that `homing_max_wait_ms` is long enough
  - Check logs for "Homing complete timeout" warnings
- **Verify signal**: Use an oscilloscope to confirm the homing pin timing matches your configuration
- **Check wiring**: Verify the homing pin is connected to the motor's trigger input
- **Motor settings**: Ensure the motor is configured to respond to the homing signal correctly

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
| Homing method | Move to limit | Signal-based (hold or pulse) |
| Alarm monitoring | Via separate pins | Integrated |
| Timeout-based homing | ✗ | ✓ |
| Pulse-triggered homing | ✗ | ✓ |
| Custom alarm actions | ✗ | ✓ |

## See Also

- Example configuration: `example_configs/signal_homing_stepper_example.yaml`
- Source code: `FluidNC/src/Motors/SignalHomingStepper.h` and `.cpp`
- FluidNC Homing documentation
- Motor driver-specific documentation for your hardware

## Version History

- **v1.2** (2025): Added event-driven homing complete detection
  - Optional homing complete pin for pulse mode
  - Configurable polarity for complete signal
  - Safety timeout with `homing_max_wait_ms`
  - Polls complete pin every 10ms for responsive completion
  - Falls back to time-based completion if pin not configured

- **v1.1** (2025): Added dual homing modes
  - Hold mode: Signal stays active for entire homing duration
  - Pulse mode: Brief pulse followed by settle period
  - Separate configuration for signal and settle durations
  - Improved logging with mode-specific messages

- **v1.0** (2025): Initial implementation
  - Signal-based homing
  - Configurable alarm pin
  - Multiple alarm actions (estop, pause, macros)
