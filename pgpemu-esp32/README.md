ESP-IDF Gatt Server Service Table Demo
===============================================

Before the first build, copy `main/secrets.example.c` to `main/secrets.c`,
replace the dummy values with your device's credentials, and remove the
template's `#error` line. `secrets.c` is ignored by Git; keep real credentials
out of `secrets.example.c`. Existing local `secrets.c` files can be reused.

The dashboard uses 25% backlight brightness and turns the backlight off after
15 seconds without a BOOT-button press. The first press while dark wakes the
screen without changing the automatic mode; further presses cycle modes and
restart the timeout. Catching, spinning, and counter saves continue while dark.

This demo shows how to create a GATT service with an attribute table defined in one place. Provided API releases the user from adding attributes one by one as implemented in BLUEDROID. A demo of the other method to create the attribute table is presented in [gatt_server_demo](../gatt_server).

Please check the [tutorial](tutorial/Gatt_Server_Service_Table_Example_Walkthrough.md) for more information about this example.
