# Flashing + Monitor (ESP-IDF)

Quick reminder for this project.

Windows PowerShell (USB -> WSL2) if the device is not visible:

```
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --busid <BUSID> --wsl
usbipd detach --busid <BUSID>
```

1) Load ESP-IDF environment (adjust path if needed):

```
source ~/esp/esp-idf/export.sh
```

2) Flash + monitor:

```
idf.py -p /dev/ttyACM0 flash monitor
```

Optional: clean build + full chip erase (use if the device is acting weird):

```
idf.py -p /dev/ttyACM0 erase_flash
idf.py -p /dev/ttyACM0 fullclean
idf.py -p /dev/ttyACM0 flash monitor
```
