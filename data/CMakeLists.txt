configure_file(kyu-check.service.in ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service @ONLY)
configure_file(kyu-tray.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop @ONLY)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service
    kyu-check.timer
    DESTINATION ${KYU_SYSTEMD_USER_UNIT_DIR})
# Started at login by the desktop's XDG autostart (Plasma, and most others).
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart)
