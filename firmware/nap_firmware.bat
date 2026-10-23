@echo off
chcp 65001 >nul
echo ========================================================
echo   NẠP FIRMWARE XIAOZHI CUSTOM CHO ESP32-S3 SUPER MINI
echo   (OLED: 3/4, Loa: 5/6/7, Mic: 8/9/10, Button: BOOT 0)
echo ========================================================
set /p port="Nhập cổng COM (ví dụ COM61): "
if "%port%"=="" set port=COM61

echo.
echo Đang nạp firmware vào %port% ...
python -m esptool --port %port% --chip esp32s3 -b 921600 write-flash 0x0 xiaozhi_custom_s3mini_all_in_one.bin

echo.
echo Nạp hoàn tất! Bấm phím bất kỳ để thoát.
pause