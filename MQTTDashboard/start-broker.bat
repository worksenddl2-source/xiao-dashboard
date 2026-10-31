@echo off
rem Start the local Mosquitto broker (TCP 1884 for the board, WebSocket 9001 for the browser)
cd /d "%~dp0"
"C:\Program Files\mosquitto\mosquitto.exe" -c mosquitto.conf -v
