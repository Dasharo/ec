# Adding a new board

## Charger parameters

- `CHARGER_ADAPTER_RSENSE`: The adapter RSENSE value in milliohms.
- `CHARGER_BATTERY_RSENSE`: The battery RSENSE value in milliohms.
- `CHARGER_CHARGE_CURRENT`: The desired charge current in milliamps.
- `CHARGER_CHARGE_VOLTAGE`: On the battery, look for 充电限制电压 (charge limit
  voltage). Convert this from volts to millivolts.
- `CHARGER_INPUT_CURRENT`: On the charger, look for DC output. Convert the
  current from amps to milliamps.
- `BATTERY_CHARGE_TEMP_MAX` / `BATTERY_CHARGE_TEMP_RESUME` (optional): Battery
  temperature, in 0.1 K, at which charging is inhibited and allowed again.
  Defaults to 45 °C / 40 °C. On the battery, look for the charge temperature
  range (充电温度) and keep the maximum at or below its upper bound. Use
  `BATTERY_TEMP_DK(c)` to convert from °C. The EC reads the pack's
  `Temperature()` (SBS 0x08) and also honors the `OVER_TEMP_ALARM` bit of
  `BatteryStatus()`. Check the `Battery:` section of the EC debug log on AC
  plug-in to confirm that the pack reports a real temperature.

#### Example

The gaze15 battery has:

```
充电限制电压: 16.8Vdc
```

Its charger has:

```
DC OUTPUT (输出/輸出): 19.5V⎓9.23A 180W
```

The schematics show it uses a 0.005 ohm sense resistor for both the adapter and
the battery.

This gives:

```
CFLAGS+=\
	-DCHARGER_ADAPTER_RSENSE=5 \
	-DCHARGER_BATTERY_RSENSE=5 \
	-DCHARGER_CHARGE_CURRENT=3072 \
	-DCHARGER_CHARGE_VOLTAGE=16800 \
	-DCHARGER_INPUT_CURRENT=9230
```

## GPIOs

Use ecsim on the proprietary firmware to generate the configuration. Modify
`src/main.rs` if the EC needs to be changed from 5570 to 8587. Refer to
`gpio.sh`, which can automate the process.

Use the EC page in the schematics to label everything.

Not everything will be generated correctly, so check them manually when
labeling. Examples include the M block not being included in the generated
output, and `CCD_EN` being configured as `GPIO_IN` instead of `GPIO_OUT`.

## Keyboard backlight

Many models have backlit or RGB keyboards. The EC page in the board schematics
should be sufficient to determine the method used to control the backlight.

### DAC

One of the DACs is be used for controlling the backlight level.

Examples:

- galp4
- lemp9

These models use `KBLIGHT_ADJ` to control the backlight level.

### PWM

One PWM line is used for controlling brightness, and three others are used for
setting the red, green, and blue colors.

Examples:

- addw2
- gaze15
- oryp6

These models use `EC_PWM_LEDKB_{R,G,B}` to control the color of the keys.

### I2C

I2C is used to communicate with the MCU that controls the keyboard backlight
and color.

Examples:

- darp6
- oryp5
