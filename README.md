# ZMK Firmware for PH Keyboards

This repository contains ZMK firmware configurations for PH Design keyboards.

## My Layout

HHKB layout & 3+1+3 split spacebar.

<img width="1362" height="598" alt="Screenshot 2026-06-13 at 20 39 45" src="https://github.com/user-attachments/assets/c2137521-3f77-4bb9-b46c-058cf124d961" />


<img width="1366" height="600" alt="Screenshot 2026-06-13 at 20 38 28" src="https://github.com/user-attachments/assets/7a1184f6-beec-4533-adda-ad5f438b82b2" />

<img width="1364" height="599" alt="Screenshot 2026-06-13 at 20 38 36" src="https://github.com/user-attachments/assets/14795566-fc27-49b6-a806-88218d4476f1" />

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
