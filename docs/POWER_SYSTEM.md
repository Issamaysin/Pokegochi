# Pokegochi battery and charging design

## Electrical target

- Board: ESP32-2432S028R (CYD), powered from regulated 5 V.
- Design peak allowance: 1 A at 5 V even though normal draw should be much lower.
- Battery: one protected 3.7 V Li-ion/LiPo cell (1S), 4.2 V fully charged.
- Preferred capacity: 3000 mAh; acceptable compact option: 2000 mAh.
- Charging input: USB-C 5 V.
- The game may remain powered while charging.
- There is no power or screen button. The device remains powered like a virtual pet;
  display sleep and wake are controlled exclusively by the touchscreen.

## Recommended architecture (reliable load sharing)

```text
USB-C 5 V
   |
   v
MCP73871 charger + power-path/load-sharing board
   | BAT ---------------- protected 1S LiPo 3.7 V
   | SYS
   v
MT3608 boost converter adjusted to 5.00 V
   |
   +---- 470 uF low-ESR capacitor ---- GND
   |
   +---- CYD P1/VIN (5 V)
GND ---------------------------------- CYD P1/GND
```

The MCP73871 separates system load from battery charging, so the charge cycle can
terminate correctly while the game is running. The boost converter always sees the
power-path output, avoiding a topology change between USB and battery operation.

## Battery gauge used by the firmware

The current firmware reads the cell through ADC1 on GPIO35, exposed on the CYD P3
header. Wire the measurement separately from the 5 V power path:

```text
Li-ion BAT+ ---- 100 kOhm ----+---- GPIO35 (P3)
                              |
                            100 kOhm
                              |
BAT-/system GND --------------+---- CYD GND
```

Add a 100 nF ceramic capacitor between the GPIO35 junction and GND close to the
board. At a fully charged 4.2 V cell the GPIO receives about 2.1 V. Never connect
the cell directly to GPIO35. Battery/BMS ground and CYD ground must be common.

The firmware takes four readings 200 ms apart, averages them, and maps the result
through a piecewise Li-ion discharge curve. Home intentionally shows four coarse
battery bars instead of a numeric percentage because an ADC/divider estimate varies
under charging and display/SD load. A MAX17048 can still replace it later for a more
accurate fuel gauge, but is not required for the installed battery prototype.

## Candidate AliExpress components

Listings change frequently; item IDs below identify the researched product family.
Confirm connector, dimensions, protection PCB, charge current, and review photos
before purchase.

### Battery

- Recommended size: 103665, 3.7 V, nominal 3000 mAh, approximately 10 x 36 x 65 mm.
  AliExpress item `1005011988289116`.
- Flatter alternative: 505573/505570, 3.7 V, nominal 3000 mAh, approximately
  5 x 55 x 73 mm. AliExpress item `1005008070864068`.
- Compact alternative: 103450, 3.7 V, nominal 2000 mAh, approximately 10 x 34 x 50 mm,
  JST-PH 2.0 option. AliExpress item `1005011829405264`.

The cell must include a 1S protection PCB for overcharge, over-discharge, and
over-current. If the listing cannot confirm protection, do not use it without a
separate 1S protection board.

### Charger / power path

- MCP73871 load-sharing charger board: AliExpress item `1005006871528933`.
- Similar MCP73871 USB charger board: AliExpress item `1005009322946345`.

### 5 V boost

- MT3608 adjustable boost module: AliExpress item `1005007952646165`.
- Smaller generic MT3608 option: AliExpress item `4000826731809`.

Adjust the module to exactly 5.00 V with a multimeter before connecting the CYD.
Do not adjust it while connected to the board.

### Compact alternative

An IP5306 all-in-one charge/boost module reduces the part count and provides 5 V
at up to roughly 2 A depending on the implementation. Candidate item:
`1005007337536631`. It is acceptable for a compact prototype, but some power-bank
boards can briefly interrupt output when USB power is connected or removed. The
MCP73871 + boost architecture is preferred for the final unit.

## Controls and connectors

- No external control button or master disconnect is fitted.
- The XPT2046 touch IRQ wakes the ESP32 and display from light sleep. Wake opens the
  dedicated lock screen; the player must slide its Poke Ball across the track before
  gameplay controls accept input. Settings also has a manual lock button.
- Charge through the new USB-C port connected to the charger, not through the CYD's
  Micro-USB port.
- During bench firmware uploads, disconnect the external battery/power-path wiring
  before powering the CYD through Micro-USB. The final service connection must not
  tie two independent 5 V sources together.

## Bring-up sequence

1. Test the charger and protected cell without the CYD.
2. Verify charge termination and battery temperature.
3. Set boost output to 5.00 V with no CYD connected.
4. Load-test 5 V with at least 500 mA and verify voltage remains stable.
5. Add the 470 uF capacitor.
6. Connect VIN and GND to the CYD with the polarity checked twice.
7. Measure real current at full backlight, SD access, and Bluetooth activity.
8. Only then enable charging while playing and validate USB plug/unplug behavior.

## Expected autonomy before measurement

These are engineering estimates, not guarantees:

- 2000 mAh: roughly 4-7 hours.
- 3000 mAh: roughly 6-10 hours.

Actual runtime will depend heavily on backlight brightness, Bluetooth/Wi-Fi use,
SD activity, regulator efficiency, and the honesty of the advertised cell capacity.
