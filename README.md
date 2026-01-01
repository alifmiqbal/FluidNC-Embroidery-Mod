# FluidNC Embroidery Mod

This is a modified version of [FluidNC](https://github.com/bdring/FluidNC) designed specifically for controlling embroidery machines. It introduces a synchronization mechanism that coordinates X/Y pantograph movements with the needle's position using a sensor.

## Key Features

-   **Needle Synchronization**: The firmware waits for a signal from a needle position sensor (e.g., an optical interrupter or Hall effect sensor) before executing the next stitch movement. This ensures the needle is out of the fabric before the frame moves.
-   **Stitch Buffering**: Stitches are queued in a dedicated buffer (FreeRTOS queue) to ensure smooth operation without stalling the G-code parser.
-   **Burst Movement**: Stitch movements are executed as rapid "bursts" of steps to maximize speed between needle penetrations.
-   **Embroidery Mode**: A dedicated mode that handles the specific requirements of embroidery G-code execution.

## Configuration

To use the embroidery features, you need to define the `needle_sensor_pin` in your machine configuration YAML file.

Example `config.yaml` snippet:

```yaml
embroidery:
  needle_sensor_pin: gpio.34:low  # Example pin, adjust to your wiring
```

-   **needle_sensor_pin**: The GPIO pin connected to your needle position sensor. The firmware triggers the move on the **FALLING** edge of this signal (by default).

## How It Works

1.  **G-Code Parsing**: The firmware parses G-code commands (standard G0/G1 moves).
2.  **Queueing**: Instead of executing moves immediately or adding them to the standard motion planner, embroidery stitches are sent to a special `StitchQueue`.
3.  **Waiting**: A background task waits for the needle sensor interrupt.
4.  **Trigger**: When the needle sensor triggers (indicating the needle is up), the task pulls the next stitch from the queue.
5.  **Execution**: The X and Y motors are stepped rapidly ("Burst Move") to the new position before the needle comes down again.

## Usage

1.  **Hardware**: Connect your needle position sensor to the configured ESP32 pin.
2.  **Firmware**: Flash this modified firmware to your ESP32.
3.  **Config**: Upload your config file with the `needle_sensor_pin` defined.
4.  **G-Code**: Send your embroidery G-code design. The machine will move the frame only when the needle sensor is triggered.

## Disclaimer

This is experimental firmware.
-   **Use at your own risk.**
-   Ensure your machine is mechanically capable of the rapid movements required.
-   Always test with the needle thread removed first to verify synchronization.

## Original FluidNC

For general FluidNC documentation, please refer to the [official Wiki](http://wiki.fluidnc.com).
