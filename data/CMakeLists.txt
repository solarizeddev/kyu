configure_file(kyu-check.service.in ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service @ONLY)
configure_file(kyu-tray.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop @ONLY)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kyu-check.service
    kyu-check.timer
    DESTINATION ${KYU_SYSTEMD_USER_UNIT_DIR})
# Started at login by the desktop's XDG autostart (Plasma, and most others).
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${KYU_APP_ID}.tray.desktop
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart)

# Privileged helper: system unit, D-Bus activation and policy, polkit action.
if(KYU_HELPER)
    set(KYU_SYSTEMD_SYSTEM_UNIT_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system"
        CACHE PATH "Where the systemd system units are installed")
    configure_file(kyu-helper.service.in ${CMAKE_CURRENT_BINARY_DIR}/kyu-helper.service @ONLY)
    configure_file(io.github.solarizeddev.kyu.Helper.service.in
                   ${CMAKE_CURRENT_BINARY_DIR}/io.github.solarizeddev.kyu.Helper.service @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kyu-helper.service DESTINATION ${KYU_SYSTEMD_SYSTEM_UNIT_DIR})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/io.github.solarizeddev.kyu.Helper.service
            DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services)
    install(FILES io.github.solarizeddev.kyu.Helper.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system.d)
    install(FILES io.github.solarizeddev.kyu.policy DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
    install(FILES io.github.solarizeddev.kyu.Helper1.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces)
endif()
