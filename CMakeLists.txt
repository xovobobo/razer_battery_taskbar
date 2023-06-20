cmake_minimum_required(VERSION 3.20)

project(RazerBatteryTaskbar)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

add_executable(razer_battery_tray src/razer_battery_tray.cpp)
target_link_libraries(razer_battery_tray PRIVATE ${LIBUSB_LIBRARIES} -lusb-1.0 comctl32)

include_directories(razer_battery_tray PRIVATE ${LIBUSB_INCLUDE_DIRS})