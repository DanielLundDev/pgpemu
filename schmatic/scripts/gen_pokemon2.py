import json
import re
import uuid
from pathlib import Path


def _find_block_end(text: str, start_token: str) -> int:
    start = text.find(start_token)
    if start == -1:
        raise ValueError(f"Missing block: {start_token}")

    depth = 0
    for idx in range(start, len(text)):
        if text[idx] == "(":
            depth += 1
        elif text[idx] == ")":
            depth -= 1
            if depth == 0:
                return idx

    raise ValueError(f"Unterminated block: {start_token}")


def _uuid() -> str:
    return str(uuid.uuid4())


def _fmt_coord(value: float) -> str:
    return f"{value:.2f}".rstrip("0").rstrip(".")


def _symbol_instance(lib_id: str, ref: str, value: str, at: str, pin_count: int, root_uuid: str) -> str:
    symbol_uuid = _uuid()
    pin_uuids = [_uuid() for _ in range(pin_count)]
    x_str, y_str, rot = at.split()
    x = float(x_str)
    y = float(y_str)
    ref_y = _fmt_coord(y - 3.81)
    val_y = _fmt_coord(y - 1.27)
    x_fmt = _fmt_coord(x)
    y_fmt = _fmt_coord(y)
    at_fmt = f"{x_fmt} {y_fmt} {rot}"
    pin_lines = "\n".join(
        f'\t\t(pin "{i + 1}"\n\t\t\t(uuid "{pin_uuids[i]}")\n\t\t)\n'
        for i in range(pin_count)
    ).rstrip("\n")

    return (
        "\t(symbol\n"
        f'\t\t(lib_id "{lib_id}")\n'
        f"\t\t(at {at_fmt})\n"
        "\t\t(unit 1)\n"
        "\t\t(exclude_from_sim no)\n"
        "\t\t(in_bom yes)\n"
        "\t\t(on_board yes)\n"
        "\t\t(dnp no)\n"
        f'\t\t(uuid "{symbol_uuid}")\n'
        f'\t\t(property "Reference" "{ref}"\n'
        f"\t\t\t(at {x_fmt} {ref_y} 0)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        f'\t\t(property "Value" "{value}"\n'
        f"\t\t\t(at {x_fmt} {val_y} 0)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        '\t\t(property "Footprint" ""\n'
        f"\t\t\t(at {x_fmt} {y_fmt} 0)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t\t(hide yes)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        '\t\t(property "Datasheet" ""\n'
        f"\t\t\t(at {x_fmt} {y_fmt} 0)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t\t(hide yes)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        '\t\t(property "Description" ""\n'
        f"\t\t\t(at {x_fmt} {y_fmt} 0)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t\t(hide yes)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        f"{pin_lines}\n"
        "\t\t(instances\n"
        '\t\t\t(project ""\n'
        f'\t\t\t\t(path "/{root_uuid}"\n'
        f'\t\t\t\t\t(reference "{ref}")\n'
        "\t\t\t\t\t(unit 1)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)\n"
        "\t)\n"
    )


def main() -> None:
    repo_root = Path(__file__).resolve().parents[2]
    base_dir = repo_root / "schmatic" / "pokemon"
    out_dir = repo_root / "schmatic" / "pokemon2"
    out_dir.mkdir(parents=True, exist_ok=True)

    base_sch = base_dir / "pokemon.kicad_sch"
    base_pro = base_dir / "pokemon.kicad_pro"

    out_sch = out_dir / "pokemon2.kicad_sch"
    out_pro = out_dir / "pokemon2.kicad_pro"

    out_sch.write_text(base_sch.read_text(encoding="utf-8"), encoding="utf-8")

    project_data = json.loads(base_pro.read_text(encoding="utf-8"))
    project_data["meta"]["filename"] = out_pro.name
    out_pro.write_text(json.dumps(project_data, indent=2) + "\n", encoding="utf-8")

    text = out_sch.read_text(encoding="utf-8")
    text = text.replace('project "pokemon"', 'project "pokemon2"')

    root_uuid_match = re.search(r'\(uuid "([^"]+)"\)', text)
    if not root_uuid_match:
        raise ValueError("Unable to locate root UUID in schematic.")
    root_uuid = root_uuid_match.group(1)

    new_lib_symbols = """
\t\t(symbol "Custom:Battery_1S"
\t\t\t(pin_names
\t\t\t\t(offset 0.254)
\t\t\t)
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "BAT"
\t\t\t\t(at 0 3.81 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Value" "Battery_1S"
\t\t\t\t(at 0 -3.81 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Footprint" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Datasheet" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Description" "1S LiPo/Li-ion battery"
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "Battery_1S_0_1"
\t\t\t\t(rectangle
\t\t\t\t\t(start -1.27 2.54)
\t\t\t\t\t(end 1.27 -2.54)
\t\t\t\t\t(stroke
\t\t\t\t\t\t(width 0.254)
\t\t\t\t\t\t(type default)
\t\t\t\t\t)
\t\t\t\t\t(fill
\t\t\t\t\t\t(type none)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "Battery_1S_1_1"
\t\t\t\t(pin passive line
\t\t\t\t\t(at -3.81 1.27 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "+")
\t\t\t\t\t(number "1"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at -3.81 -1.27 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "-")
\t\t\t\t\t(number "2"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(embedded_fonts no)
\t\t)
\t\t(symbol "Custom:SSD1306_OLED_I2C"
\t\t\t(pin_names
\t\t\t\t(offset 0.254)
\t\t\t)
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "DS"
\t\t\t\t(at 0 7.62 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Value" "SSD1306_OLED_I2C"
\t\t\t\t(at 0 -7.62 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Footprint" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Datasheet" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Description" "SSD1306 OLED module (I2C)"
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "SSD1306_OLED_I2C_0_1"
\t\t\t\t(rectangle
\t\t\t\t\t(start -2.54 6.35)
\t\t\t\t\t(end 7.62 -6.35)
\t\t\t\t\t(stroke
\t\t\t\t\t\t(width 0.254)
\t\t\t\t\t\t(type default)
\t\t\t\t\t)
\t\t\t\t\t(fill
\t\t\t\t\t\t(type none)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "SSD1306_OLED_I2C_1_1"
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 3.81 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "GND")
\t\t\t\t\t(number "1"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 1.27 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "VCC")
\t\t\t\t\t(number "2"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 -1.27 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "SCL")
\t\t\t\t\t(number "3"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 -3.81 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "SDA")
\t\t\t\t\t(number "4"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(embedded_fonts no)
\t\t)
\t\t(symbol "Custom:Fuse_2Pin"
\t\t\t(pin_numbers
\t\t\t\t(hide yes)
\t\t\t)
\t\t\t(pin_names
\t\t\t\t(offset 0)
\t\t\t)
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "F"
\t\t\t\t(at 2.032 0 90)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Value" "Fuse_2Pin"
\t\t\t\t(at 0 0 90)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Footprint" ""
\t\t\t\t(at -1.778 0 90)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Datasheet" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Description" "Fuse or polyfuse"
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "Fuse_2Pin_0_1"
\t\t\t\t(rectangle
\t\t\t\t\t(start -1.016 -2.54)
\t\t\t\t\t(end 1.016 2.54)
\t\t\t\t\t(stroke
\t\t\t\t\t\t(width 0.254)
\t\t\t\t\t\t(type default)
\t\t\t\t\t)
\t\t\t\t\t(fill
\t\t\t\t\t\t(type none)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "Fuse_2Pin_1_1"
\t\t\t\t(pin passive line
\t\t\t\t\t(at 0 3.81 270)
\t\t\t\t\t(length 1.27)
\t\t\t\t\t(name "~")
\t\t\t\t\t(number "1"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at 0 -3.81 90)
\t\t\t\t\t(length 1.27)
\t\t\t\t\t(name "~")
\t\t\t\t\t(number "2"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(embedded_fonts no)
\t\t)
\t\t(symbol "Custom:USB_ESD_Dual"
\t\t\t(pin_names
\t\t\t\t(offset 0.254)
\t\t\t)
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "D"
\t\t\t\t(at 0 6.35 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Value" "USB_ESD_Dual"
\t\t\t\t(at 0 -6.35 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Footprint" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Datasheet" ""
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(property "Description" "USB D+/- ESD protection array"
\t\t\t\t(at 0 0 0)
\t\t\t\t(effects
\t\t\t\t\t(font
\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t)
\t\t\t\t\t(hide yes)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "USB_ESD_Dual_0_1"
\t\t\t\t(rectangle
\t\t\t\t\t(start -2.54 5.08)
\t\t\t\t\t(end 2.54 -5.08)
\t\t\t\t\t(stroke
\t\t\t\t\t\t(width 0.254)
\t\t\t\t\t\t(type default)
\t\t\t\t\t)
\t\t\t\t\t(fill
\t\t\t\t\t\t(type none)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(symbol "USB_ESD_Dual_1_1"
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 2.54 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "D+")
\t\t\t\t\t(number "1"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at -5.08 0 0)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "D-")
\t\t\t\t\t(number "2"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t\t(pin passive line
\t\t\t\t\t(at 0 -7.62 90)
\t\t\t\t\t(length 2.54)
\t\t\t\t\t(name "GND")
\t\t\t\t\t(number "3"
\t\t\t\t\t\t(effects
\t\t\t\t\t\t\t(font
\t\t\t\t\t\t\t\t(size 1.27 1.27)
\t\t\t\t\t\t\t)
\t\t\t\t\t\t)
\t\t\t\t\t)
\t\t\t\t)
\t\t\t)
\t\t\t(embedded_fonts no)
\t\t)
"""

    lib_end = _find_block_end(text, "(lib_symbols")
    text = text[:lib_end] + new_lib_symbols + text[lib_end:]

    new_symbols = (
        _symbol_instance("Custom:Battery_1S", "BAT1", "LiPo_1S", "284.48 132.08 0", 2, root_uuid)
        + _symbol_instance("Custom:SSD1306_OLED_I2C", "DS1", "SSD1306_OLED_I2C", "76.2 160.02 0", 4, root_uuid)
        + _symbol_instance("Custom:Fuse_2Pin", "F1", "Polyfuse", "248.92 101.6 0", 2, root_uuid)
        + _symbol_instance("Custom:USB_ESD_Dual", "D1", "USB_ESD", "238.76 69.85 0", 3, root_uuid)
        + _symbol_instance("Device:R", "R7", "4.7k", "93.98 152.4 90", 2, root_uuid)
        + _symbol_instance("Device:R", "R8", "4.7k", "101.6 152.4 90", 2, root_uuid)
        + _symbol_instance("Device:R", "R9", "10k", "256.54 140.97 0", 2, root_uuid)
        + _symbol_instance("Device:C_US", "C3", "0.1uF", "93.98 119.38 90", 2, root_uuid)
        + _symbol_instance("Device:C_US", "C4", "0.1uF", "101.6 119.38 90", 2, root_uuid)
        + _symbol_instance("Device:C_US", "C5", "10uF", "109.22 119.38 90", 2, root_uuid)
    )

    sheet_marker = "\n\t(sheet_instances"
    marker_index = text.find(sheet_marker)
    if marker_index == -1:
        raise ValueError("Unable to find sheet_instances marker.")
    text = text[:marker_index] + "\n" + new_symbols + text[marker_index:]

    out_sch.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
