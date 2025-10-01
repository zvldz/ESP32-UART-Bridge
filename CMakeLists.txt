cmake_minimum_required(VERSION 3.16)
set(EXCLUDE_COMPONENTS
    "esp-zigbee-lib;esp-zboss-lib;bt;qrcode;esp_rainmaker;rmaker_common;libsodium;esp_insights;esp_diagnostics;esp_diag_data_store;esp_schedule;esp_secure_cert_mgr;esp_modem;esp_rcp_update"
    CACHE STRING "" FORCE)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-uart-bridge)