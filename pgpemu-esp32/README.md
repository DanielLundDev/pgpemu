ESP-IDF Gatt Server Service Table Demo
===============================================

Before the first build, copy `main/secrets.example.c` to `main/secrets.c`,
replace the dummy values with your device's credentials, and remove the
template's `#error` line. `secrets.c` is ignored by Git; keep real credentials
out of `secrets.example.c`. Existing local `secrets.c` files can be reused.

This demo shows how to create a GATT service with an attribute table defined in one place. Provided API releases the user from adding attributes one by one as implemented in BLUEDROID. A demo of the other method to create the attribute table is presented in [gatt_server_demo](../gatt_server).

Please check the [tutorial](tutorial/Gatt_Server_Service_Table_Example_Walkthrough.md) for more information about this example.
