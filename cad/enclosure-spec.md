# Pokemon GO device enclosure — draft specifications

Status: component measurements collected; layout and mechanical details pending.
All dimensions are millimeters. These are component envelopes, not finished
pocket dimensions; printing clearance and wiring space must be added.

## Components

| Component | Length | Width | Thickness | Source |
| --- | ---: | ---: | ---: | --- |
| Battery | 65 | 36 | 10 | https://www.amazon.com/dp/B0FH9XHXRT |
| USB-C charging/boost board | 25 | 20 | 4.5 | https://www.amazon.com/dp/B09YD5C9QC |
| ESP32/display assembly | 36.6 | 20.5 | 15 | https://www.aliexpress.us/item/3256809152126554.html |

Measurements supplied by the owner. The battery listing also specifies
36 x 10 x 65 mm, 3.7 V, 3000 mAh, and a JST 1.25 connector. The AliExpress
listing could not be retrieved; ESP connector, screen and mounting geometry
still need measurements or a dimensioned drawing of the actual assembly.

## Proposed layout, awaiting selection

- Compact option: battery in the lower compartment, with the ESP/display and
  charger arranged end-to-end on a separate upper support tray. Their combined
  lengths are 61.6 mm before clearance and wiring allowance.
- Flatter option: battery and electronics occupy adjacent areas, trading a
  larger footprint for less thickness.
- Keep the display visible and BOOT accessible. Provide charger USB-C access
  and confirm whether the ESP USB port and RESET also need external access.
- Support the boards independently of the battery. Allow space for battery
  leads, insulation, connector bodies, and cable bends.
- FreeCAD is the proposed CAD tool; no final model or print dimensions yet.

## Confirmed printing and closure requirements

- Printer: Creality K1C.
- Material: PLA.
- Closure: screws into M3 heat-set inserts, rather than snap-fit tabs.
- Screws: M3 x 4 mm (3 mm thread diameter, 4 mm screw length), hex drive.
- Inserts: M3 x 4 x 4.2, interpreted as M3 thread, 4 mm insert length,
  and 4.2 mm outside diameter. Confirm against the insert drawing before
  finalizing hole geometry.
- Screw head profile/dimensions and nozzle diameter are still to be confirmed.
- Defer the charger power button. Do not add its opening or actuator to the
  initial case; it can be added later if needed. This does not remove BOOT
  access for mode selection. The display remains on at 25% brightness.

## Details to confirm

- Whether the ESP's 15 mm thickness includes headers, acrylic, and spacers
  that will remain installed.
- Screen/window dimensions and offset from board edges; mounting-hole
  diameters and centers; connector/button positions and heights.
- Battery lead exit and connector envelope; actual wiring/power routing.
- Nozzle size and screw head profile/dimensions.
- Desired overall shape and any lanyard or pocket clip.

## Power-control consideration

The charger listing says the 5 V output switches off when its load remains
below 50 mA. It also describes an external key connected between K and output
negative, with a short press enabling output and a double press disabling it.
The duration of the low-current cutoff is not specified in the retrieved text.
Low-load compatibility has not been measured. Per the owner's instruction,
this is deferred and does not block the initial enclosure design.
