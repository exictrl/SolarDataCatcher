# SolarDataCatcher v2.0 

## Overview
A real-time solar data monitoring application that fetches solar wind, flare probabilities, magnetometer data, and Kp-index from NOAA APIs and sends them via OSC.

## Credits
Developed by Elizaveta Fomina
  KVEF art & science research group
Special thanks to NOAA for providing the solar data APIs

## Installation & Usage 
1. Unzip SolarDataCatcher.zip
2. In Terminal, navigate to the folder containing the program file:
cd ~/Downloads/SolarDataCatcher
3. Run: ./install.sh
4. To prevent Terminal from showing "There are active processes" warnings when closing the window:
    - Open Terminal Preferences
    - Go to Profiles tab
    - Select your active profile
    - Go to Shell section
    - Under When the shell exits select:
        - Never ask before closing

COMMANDS:
    - Start: solar-watcher
    - Stop: Ctrl+C

The program sends data to port 6000 every 60 seconds.
Upon launch, a Terminal window with logs will appear.