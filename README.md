# Adaptive Urban Heat Shelter
### A Sensor-Driven, Self-Regulating Public Shelter for Urban Heat Resilience

> **Project status:** Prototype design / planned implementation. Performance figures, thresholds, and safety compliance must be verified through testing; this repository does not claim a certified or field-validated product.

## 1. Overview

The **Adaptive Urban Heat Shelter** is a proposed small-scale smart shelter that responds to hot outdoor conditions. It monitors temperature, humidity, sunlight intensity, and occupancy, then uses a microcontroller to coordinate a **motorized louvered roof with a reflective-panel mechanism**, a ventilation fan, and a water-misting pump.

The aim is to make waiting areas such as bus stops and public waiting shelters more comfortable during hot weather while avoiding unnecessary energy and water use when nobody is present.

## 2. Problem Statement

Conventional public shelters often provide only fixed shade and limited airflow. During periods of high temperature and strong sunlight, the shelter may still become uncomfortable for passengers, outdoor workers, older adults, and children. A passive shelter cannot automatically adjust its shade or activate cooling equipment based on current conditions.

## 3. Proposed Solution

The system uses sensors to monitor the surrounding environment and occupancy. An Arduino UNO processes the sensor readings and applies rule-based control logic. According to the detected conditions, it can:

- Adjust the **louvered roof and reflective-panel mechanism** to reduce direct solar exposure.
- Turn on a 12 V ventilation fan when cooling airflow is needed.
- Operate a 12 V misting pump only when conditions and occupancy allow.
- Disable unnecessary cooling when the shelter is unoccupied.
- Enter a safe state if a sensor reading is invalid or the power supply is unstable.

The initial version is intended as a **tabletop proof of concept**, not as a finished public installation.

## 4. Main Features

- Environmental monitoring using temperature, humidity, and ambient-light sensors.
- Occupancy-aware operation.
- Servo-based roof/panel actuation.
- 12 V fan and water-pump switching using MOSFET driver stages.
- Separate 12 V actuator supply and regulated 5 V electronics supply.
- Labelled schematic nets to make wiring easier to trace.
- No LCD required; sensor readings and debug messages can be viewed through the Arduino Serial Monitor during development.

## 5. System Architecture

```text
                 ENVIRONMENT
      Temperature / Humidity / Sunlight
                    Occupancy
                        |
                        v
          +---------------------------+
          |       SENSOR INPUTS       |
          | DHT22 | BH1750 | HC-SR501 |
          +---------------------------+
                        |
                        v
          +---------------------------+
          |       ARDUINO UNO         |
          | Read sensors              |
          | Estimate heat conditions  |
          | Apply control rules       |
          | Check safety conditions   |
          +---------------------------+
              |          |          |
              v          v          v
        +-----------+ +---------+ +-----------+
        | PCA9685   | | MOSFET  | | MOSFET    |
        | PWM servo | | fan     | | pump      |
        | driver    | | driver  | | driver    |
        +-----------+ +---------+ +-----------+
              |          |          |
              v          v          v
        Roof/panel      12 V       12 V
        mechanism       fan        mist pump

       12 V input --> 5 V buck converter --> Arduino/sensors
       Common ground for control and power circuits
       (verify driver and supply requirements before assembly)
```

## 6. Hardware Selection

| Ref. | Component | Purpose | Proposed connection / notes |
|---|---|---|---|
| U1 | DHT22 temperature and humidity sensor | Measures ambient temperature and relative humidity | DATA to Arduino digital input; use a 10 kΩ pull-up if required by the sensor module |
| U2 | BH1750 light-intensity sensor | Measures ambient illuminance as a sunlight proxy | I²C: SDA to A4, SCL to A5 on Arduino UNO |
| U3 | HC-SR501 PIR sensor | Detects movement/presence near the shelter | OUT to a digital input; this detects motion and may not detect a stationary person reliably |
| U4 | Arduino UNO R3 compatible board | Reads sensors and runs control logic | Main controller; no LCD is required |
| U5 | PCA9685 16-channel PWM driver | Generates servo control signals | I²C connection to Arduino; separate servo supply may be required by the selected servos |
| M1/M2 | Two servo motors | Move the linked louvered-roof / reflective-panel mechanism | PCA9685 output channels; mechanical coupling and travel limits must be tested |
| Q1 | Logic-level N-channel MOSFET, e.g. IRLZ44N | Switches the 12 V fan | Arduino PWM/control through a suitable gate resistor and gate pull-down |
| Q2 | Logic-level N-channel MOSFET, e.g. IRLZ44N | Switches the 12 V misting pump | Arduino control through a suitable gate resistor and gate pull-down |
| J3 | 12 V DC fan | Provides ventilation | Powered from the 12 V rail through its driver |
| J4 | 12 V misting pump | Supplies water to the misting nozzle | Powered from the 12 V rail through its driver |
| U8 | LM2596 buck converter module | Steps 12 V down to regulated 5 V | Adjust and measure output before connecting electronics |
| J5 | 12 V DC input, nominal 5 A adapter | Supplies the prototype | Actual adapter rating must be selected from measured peak current |
| C1/C2 | Bulk capacitors | Help smooth supply transients | Voltage and polarity ratings must be observed |
| C3/C4 | Decoupling capacitors | Reduce supply noise near electronics | Place close to the relevant power pins |
| R2/R3, R8/R9 | 4.7 kΩ I²C pull-up resistors | Pull SDA and SCL up to the logic supply if needed | Check whether sensor/driver breakout boards already include pull-ups before adding duplicates |
| R4/R6 | 220 Ω gate resistors (illustrative) | Limit MOSFET gate current spikes | Confirm suitable values during circuit review |
| R5/R7 | 10 kΩ gate pull-down resistors | Keep MOSFETs off during controller startup | Place from gate to ground |

**Important hardware note:** Component modules and pinouts vary. Confirm each datasheet and the actual breakout-board schematic before wiring. Do not power a servo, fan, or pump directly from an Arduino I/O pin.

## 7. Proposed Signal Mapping

Use these as a starting point only and confirm them against the final Fusion schematic and firmware before wiring.

| Signal | Arduino UNO connection |
|---|---|
| DHT22 data | D2 |
| HC-SR501 OUT | D3 |
| BH1750 SDA | A4 / SDA |
| BH1750 SCL | A5 / SCL |
| PCA9685 SDA | A4 / SDA |
| PCA9685 SCL | A5 / SCL |
| Fan MOSFET control | D8 |
| Pump MOSFET control | D9 |
| Servo control signals | PCA9685 output channels, for example channel 0 and channel 1 |

The BH1750 and PCA9685 share the I²C bus, so they use the same SDA and SCL lines. Give the devices distinct I²C addresses where applicable and verify address compatibility. Keep signal labels unique: `DHT_DATA`, `PIR_OUT`, `I2C_SDA`, `I2C_SCL`, `FAN_CTRL`, `PUMP_CTRL`, `SERVO_ROOF`, and `SERVO_PANEL`.

## 8. Control Logic

The first prototype should use transparent, rule-based logic. Thresholds below are **starting values for bench testing only**, not medical limits or validated heat-stress thresholds.

1. Read temperature and humidity from the DHT22.
2. Read illuminance from the BH1750.
3. Read the occupancy signal from the HC-SR501.
4. Validate sensor readings and apply basic filtering.
5. If the shelter is unoccupied, keep the misting pump off and reduce unnecessary actuator use.
6. If occupied and sunlight/temperature are high, move the louvered roof and reflective panel to the configured shade position.
7. If occupied and conditions warrant more airflow, switch on the ventilation fan.
8. Permit misting only if the pump is enabled, water is available, humidity is not too high, and the surrounding area is suitable for misting.
9. If a critical sensor reading is invalid or the supply is unstable, turn the pump off and move actuators only if it is safe to do so.
10. Log sensor values and actuator states to the Serial Monitor during development.

### Illustrative state table

| State | Example condition | Roof/panel | Fan | Misting |
|---|---|---|---|---|
| Unoccupied | No occupancy detected | Default / low-power position | Off | Off |
| Normal | Occupied, moderate conditions | Shade setting as configured | Off or low | Off |
| Hot / sunny | Occupied, high temperature or sunlight | Increase shade | On | Optional, if humidity and safety checks permit |
| Sensor fault | Invalid or missing critical reading | Safe position if possible | Conservative safe state | Off |
| Power fault | Supply outside permitted range | Stop movement safely | Off | Off |

Do not label these states as a certified heat-index or Wet Bulb Globe Temperature (WBGT) calculation unless the relevant formula, inputs, and validation have been implemented.

## 9. Power Design

- Use a regulated 12 V DC supply sized for the **measured simultaneous load** of the fan, pump, servos, and electronics.
- Use a buck converter to provide a stable 5 V rail for the Arduino and compatible sensors.
- Check the servo voltage and peak-current requirements. Some servos require a separate supply; do not assume the Arduino 5 V pin can supply them.
- Join grounds appropriately for the control signals, while routing motor/pump return currents to reduce noise in the sensor/controller ground path.
- Add an input fuse, reverse-polarity protection, strain relief, and suitable connectors before extended testing.
- Add suppression appropriate to the actual inductive loads and driver topology. Verify diode polarity and ratings before powering the circuit.
- Keep water lines and mist away from the electronics, connectors, and power adapter.

## 10. Mechanical Design

- Use a stable frame suitable for a tabletop demonstrator.
- Build the louvered roof and reflective panel as one coordinated shading subsystem; the two servos may be controlled together or with separate calibrated positions.
- Add mechanical end stops so the servos cannot force the linkage beyond its safe travel.
- Guard moving linkages and prevent finger trapping.
- Secure the fan and direct airflow away from exposed wiring.
- Mount the pump and water tubing so leaks cannot reach electrical components.
- For an outdoor version, assess wind loading, corrosion, UV exposure, drainage, vandal resistance, and anchoring before deployment.

## 11. Prototype Build Plan

1. **Bench power test:** Test the 12 V input and buck converter with a multimeter before attaching the controller.
2. **Sensor test:** Test each sensor independently and verify readings in the Serial Monitor.
3. **I²C test:** Confirm that both the BH1750 and PCA9685 are detected on the shared bus.
4. **Servo test:** Test each servo separately, then calibrate the coupled roof/panel movement with mechanical stops.
5. **Fan-driver test:** Test the MOSFET stage with the fan disconnected first, then with the correct load and supply.
6. **Pump-driver test:** Test the pump separately and check for leaks while keeping electronics dry.
7. **Integrated test:** Enable one subsystem at a time and check for resets, supply dips, sensor noise, and excessive heating.
8. **Scenario test:** Test unoccupied, normal, hot/sunny, high-humidity, sensor-disconnected, and power-loss conditions.
9. **Document results:** Record test setup, ambient conditions, current draw, response times, observed faults, and changes made.

## 12. Safety and Hazard Mitigation

| Hazard | Mitigation |
|---|---|
| Moving louvers/panel pinch points | Guards, mechanical stops, limited servo travel, low-speed bench tests |
| Electrical short or overload | Correctly rated fuse, insulated terminals, suitable wire gauge, protected enclosure |
| Water near electronics | Separate wet and electrical zones, drip loops, leak test, protected connectors |
| MOSFET or converter overheating | Check current ratings, heatsinking, ventilation, and temperature during tests |
| Sensor fault or disconnected wire | Detect invalid readings; disable misting and enter a defined safe state |
| Startup/reset causing movement | Defined output defaults and gate pull-downs; test power-up and brownout behavior |
| Unexpected pump operation | Occupancy and humidity gating, manual cut-off, pump-off default |
| Outdoor wind or weather | Do not install publicly until structural, ingress, electrical, and environmental checks are complete |

Standards such as **IEC 60529** (enclosure ingress protection classification) and **IEC 60204-1** (electrical equipment of machines) may be useful design references where applicable. Referencing a standard does not mean the prototype complies with or is certified to it.

## 13. Testing and Evaluation Metrics

Measure these during testing rather than claiming unverified results:

- Sensor reading stability and response time.
- Time taken for the roof/panel mechanism to reach its target position.
- Fan and pump current draw, plus total power consumption.
- Temperature and illuminance inside versus outside the prototype shelter.
- Water used per minute and total pump run time.
- Behavior when unoccupied and during sensor/power faults.
- Number of successful test cycles and any actuator jams.
- Reliability of occupancy detection, including its limitations for stationary users.

## 14. Limitations

- A BH1750 measures illuminance, not UV radiation or solar heat flux directly.
- An HC-SR501 is a motion sensor; a stationary occupant may not always be detected.
- A DHT22 provides ambient temperature and humidity but does not by itself calculate WBGT or fully characterize heat stress.
- Misting effectiveness depends on humidity, airflow, water quality, and local conditions.
- A tabletop prototype does not establish real-world cooling performance, structural safety, weather resistance, or public-use readiness.
- Final pin mapping, current ratings, thresholds, and mechanical dimensions must be verified during implementation.

## 15. Future Improvements

- Add a more appropriate occupancy sensor or sensor fusion for stationary occupants.
- Add a validated heat-stress measurement approach if WBGT or another formal index is required.
- Add wind/rain sensing and a weather-safe roof position.
- Add water-level sensing and a filter/service indicator for the misting system.
- Add data logging and an IoT dashboard after the local control system is reliable.
- Consider solar power only after measuring the actual energy budget.
- Conduct supervised outdoor trials and document results before proposing public deployment.

## 16. Sustainability and Intended Impact

The project is intended to contribute to more heat-resilient public spaces by combining adaptive shade, ventilation, and conditional misting. It aligns conceptually with **UN Sustainable Development Goal 11 (Sustainable Cities and Communities)** and **SDG 13 (Climate Action)**. The degree of impact must be evaluated through measured prototype and field-test data.

## 17. Repository Structure

Suggested repository layout:

```text
adaptive-urban-heat-shelter/
├── README.md
├── docs/
│   ├── system-architecture.png
│   ├── schematic.png
│   ├── user-journey-before-after.png
│   └── safety-review.png
├── hardware/
│   ├── bill-of-materials.csv
│   └── wiring-notes.md
├── firmware/
│   └── README.md
├── tests/
│   └── test-plan.md
└── LICENSE
```

Only add files that actually exist. Export your final Fusion schematic as a PNG or PDF and place it in `docs/`. Put the Arduino firmware in `firmware/` once it has been tested.

## 18. Current Status and Contributions

**Current stage:** Architecture and schematic planning for a tabletop prototype.

Contributions and feedback are welcome. Please open an issue describing the proposed change, hardware assumptions, and how it can be tested.

## 19. License

Choose a license before publishing. For example, MIT is a common option for open-source code; hardware design files may need a license suited to hardware. Do not claim a license until the repository includes the corresponding license file.
