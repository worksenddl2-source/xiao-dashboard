"""Check the BLEControl firmware from a PC with Bluetooth.

Finds the board named XIAO_ESP32S3, reads the touch value, listens to
touch notifications for 2 seconds, then turns the LED on and off.

    python -m pip install bleak
    python tools/ble_check.py
"""
import asyncio

from bleak import BleakClient, BleakScanner

DEVICE_NAME = "XIAO_ESP32S3"
TOUCH_UUID = "7f1c0002-5a3e-4c8b-9d2a-1b6e4f0a9c11"
LED_UUID = "7f1c0003-5a3e-4c8b-9d2a-1b6e4f0a9c11"


async def main():
    dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=15)
    if not dev:
        print(f"{DEVICE_NAME} not found. Is BLEControl uploaded and the board powered?")
        return
    print("found", dev.address)

    async with BleakClient(dev) as client:
        print("touch read:", (await client.read_gatt_char(TOUCH_UUID)).decode())

        received = []
        await client.start_notify(TOUCH_UUID, lambda _, data: received.append(data.decode()))
        await asyncio.sleep(2)
        await client.stop_notify(TOUCH_UUID)
        print(f"touch notifications in 2 s: {len(received)}, last = {received[-1] if received else None}")

        await client.write_gatt_char(LED_UUID, b"1", response=True)
        print("LED after writing 1:", (await client.read_gatt_char(LED_UUID)).decode())
        await asyncio.sleep(1)
        await client.write_gatt_char(LED_UUID, b"0", response=True)
        print("LED after writing 0:", (await client.read_gatt_char(LED_UUID)).decode())


asyncio.run(main())
