# Repository Guidelines

## Project Structure & Module Organization
- `pgpemu-esp32/`: ESP-IDF firmware. Core sources live in `pgpemu-esp32/main/` (`pgpemu.c`, `pgp-cert.c`, `aes.c`, `secrets.c`).
- `firmware-tools/`: Python scripts for decrypting/patching Pokemon GO Plus firmware (`decrypt.py`, `patch.py`) with usage notes in `firmware-tools/README.md`.
- Root files include project docs (`README.md`) and local key exports; treat any device data as sensitive.

## Build, Test, and Development Commands
From `pgpemu-esp32/`:
```bash
source ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
Use `idf.py -p /dev/ttyACM0 erase_flash` and `idf.py fullclean` if a device gets into a bad state (see `pgpemu-esp32/flashing.md`).

Host-side test harness:
```bash
make -f Makefile.test   # builds ./cert-test with gcc
./cert-test
```

From `firmware-tools/` (requires `pogoplus.bin` and PyCrypto/PyCryptodome):
```bash
python decrypt.py        # produces dec.bin
python patch.py patched.bin
```

## Coding Style & Naming Conventions
- C sources follow the existing ESP-IDF style: tabs for indentation, braces on the same line, `snake_case` for functions/variables, `ALL_CAPS` for macros/constants.
- Keep includes grouped: system/ESP-IDF headers first, then local headers.

## Testing Guidelines
- There is no automated test suite; `cert-test` is a manual crypto/cert verification harness.
- Keep test inputs stable when validating changes and document any output differences in the PR.

## Commit & Pull Request Guidelines
- Commit messages are short, imperative sentences (e.g., "Fix BLE reconnect", "Add keys ignore").
- PRs should describe hardware context, include relevant command output, and mention any changes to keys/credentials or flashing steps.

## Security & Configuration Tips
- Device credentials live in `pgpemu-esp32/main/secrets.c`; replace them for your device and avoid sharing real keys publicly.
- Treat key exports and firmware dumps (e.g., `pogoplus.bin`) as sensitive; add to `.gitignore` if needed.
