# ZMK Firmware for PH Keyboards

This repository contains ZMK firmware configurations for PH Design keyboards.

## My Layout

HHKB layout & 3+1+3 split spacebar.

<img width="1747" height="761" alt="Screenshot 2026-05-24 at 4 57 31" src="https://github.com/user-attachments/assets/4468ac99-210b-4528-90a7-f933a66a8101" />

<img width="1735" height="631" alt="Screenshot 2026-05-24 at 4 59 32" src="https://github.com/user-attachments/assets/0e44a08c-b054-418e-bbad-059078ab3441" />

<img width="1751" height="645" alt="Screenshot 2026-05-24 at 4 57 57" src="https://github.com/user-attachments/assets/ae1fae52-8361-4a38-bf2f-29ed5eaa9d13" />

## Download Firmware

1. Go to the **Actions** tab
2. Click on a successful build (look for the green checkmark ✓)
3. Download the firmware file from the **Artifacts** section
4. Flash it to your keyboard

## About PH-Lite

**PH-Lite** is what we call the nRF52840-based wireless controller shield used in our keyboards. 

### Why "Lite"?

When we decided to build a wireless keyboard, we spent 3 months developing a custom wireless solution and another 6 months writing the firmware from scratch. However, due to the slow progress, we decided to migrate to ZMK firmware instead. This allowed us to focus on what matters most - creating great keyboards - while leveraging the solid foundation that ZMK provides.

## Supported Keyboards

- **PH60-SC V2** - 60% keyboard with Kailh Low Profile Switches and per-key RGB underglow
- **PH60-Multi V2** - 60% keyboard with multiple layout and split spacebar options

## Features

- ZMK Studio support enabled
- USB and Bluetooth connectivity
- Battery management
- Super low power consumption
- Some custom features not in mainstream ZMK

**Team PH Design**
