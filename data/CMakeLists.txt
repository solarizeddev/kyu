configure_file(kyu-check.service.in ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service @ONLY)
configure_file(kyu-tray.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop @ONLY)
configure_file(kyu.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# The notifications' kinds, for System Settings → Notifications (the
# component is "kyu", as kyu-tray and kyu-window name it).
configure_file(kyu.notifyrc ${CMAKE_CURRENT_BINARY_DIR}/kyu.notifyrc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kyu.notifyrc DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications6)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service
    kyu-check.timer
    DESTINATION ${KYU_SYSTEMD_USER_UNIT_DIR})
# Started at login by the desktop's XDG autostart (Plasma, and most others).
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop
    DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/xdg/autostart)

# "Updating" tray icon frames (data/icons/README.md), by name in hicolor so
# Plasma colours them like theme icons.
foreach(size 22 16)
    file(GLOB _frames ${CMAKE_CURRENT_SOURCE_DIR}/icons/${size}/kyu-updating-*.svg)
    install(FILES ${_frames} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/status)
endforeach()
