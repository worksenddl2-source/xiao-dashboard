@echo off
rem Serve the dashboard at http://localhost:8080/
cd /d "%~dp0web"
python -m http.server 8080
