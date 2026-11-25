# Graph Report - ds4-translator  (2026-11-25)

## Corpus Check
- 132 files · ~148,816 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 36 file(s) not represented in the graph (top: .qml 12, (none) 8, .toml 5)
- WARNING: 36 of 173 files seen (21%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 2301 nodes · 4452 edges · 151 communities (111 shown, 40 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 536 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `25ce60b7`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- main
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- TEST_F
- check_value
- cli.py
- intercept.c
- Hardware verify.sh
- DaemonController (UI)
- ds4-ctl CLI
- TEST_F
- UI IPC client
- libudev stub tests
- ds4t-chardev.c
- HID-BPF programs
- USB HID transport (libusb)
- functionfs-backend.c
- ipc-client.cpp
- scenario_from
- Bluetooth output pacer
- AppController (UI)
- FunctionFSDevice
- TrayIcon
- hid-bpf-transport.cpp
- ioctl
- Console
- daemoncontroller.cpp
- steps
- helpers.py
- TEST_F
- lint.yml
- InputState
- trayicon.cpp
- TEST_F
- libusb_stub.cpp
- inputmonitor.cpp
- dualsense_input_report
- test_driver_transport.cpp
- ds4t-virtual.c
- QString
- DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)
- gtest
- usb_hid_transport_open
- appcontroller.cpp
- close
- src/main.cpp
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- TEST
- TEST
- jobs
- test_driver_transport.cpp
- unistd
- Security findings register
- ds4t-driver.c
- Hide methods (legacy/unbind/unlink)
- TimingStats
- udev-spoof.c
- steps
- IncludeTest
- compatibility
- ds4-translator README
- OutputState
- How DS4-Translator Works
- HideMethod
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- BtSuppression
- Bluetooth HID authorization prompt screenshot
- rules
- dualsense_touch_point
- ds4t-control.c
- hid-unbind-detect.cpp
- static_checks.sh
- compatibility-arch
- dualshock4_touch_point
- bisect-capabilities.sh
- Source
- ds4-ctl-completion.bash
- security
- FakeUI
- test-virtual.sh
- simulate_arch_release.sh
- ds4t_pad_put
- vector
- rebind-unbound-controllers.sh
- bt-disconnect.sh
- build.sh
- InputMonitor::disconnectStream
- Graphify usage rules
- steps
- dualshock4_input_report_bt
- interrupt_transfer
- ControllerType
- integration
- dualshock4_input_report_usb
- Debugging and Interceptor Guide
- analyze.sh
- run-vm.sh
- PhysicalNode
- broker.py
- transport-check.cpp
- ds4t_init
- hid-ds4-translator (kernel module)
- paths.h
- string
- ioctl
- close
- test_kernel_translate.cpp
- test_steps.py
- lib.sh
- SyntheticState
- FakeBus
- gtest
- SingleInstance
- hid-driver-sysfs.h
- fake-pad.c
- Controls
- closedir
- BatteryState
- find_hid_interface
- type_values
- TEST
- load_physical_calibration
- ReadStatus

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 62 edges
5. `TEST()` - 55 edges
6. `close()` - 47 edges
7. `AppController` - 38 edges
8. `Result` - 33 edges
9. `ioctl()` - 33 edges
10. `TransportState` - 32 edges

## Surprising Connections (you probably didn't know these)
- `on_ringbuf_report()` --calls--> `write()`  [INFERRED]
  src/hid-bpf-transport.cpp → tools/ds4-intercept/intercept.c
- `LLVMFuzzerTestOneInput()` --calls--> `parse_hid_id()`  [INFERRED]
  tests/fuzz/fuzz_parsers.cpp → src/hid-unbind-detect.cpp
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `find_physical_ds4()` --calls--> `close()`  [INFERRED]
  src/main.cpp → tools/ds4-intercept/intercept.c
- `find_physical_ds4()` --calls--> `ioctl()`  [INFERRED]
  src/main.cpp → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (151 total, 40 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.19
Nodes (7): FakeRunner, RepeatTest, RestoreTest, run_scenario(), RunStepTest, WaitForTest, Result

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "main"
Cohesion: 0.09
Nodes (39): driver_transport_destroy_virtual(), functionfs_set_mac(), backend_for_type(), backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_KERNEL (+31 more)

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (47): container, needs, permissions, runs-on, steps, needs, permissions, runs-on (+39 more)

### Community 7 - "TEST_F"
Cohesion: 0.11
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "check_value"
Cohesion: 0.22
Nodes (14): backend_values(), build_set_backend(), build_set_hide_method(), build_set_name(), check_value(), contains(), controller_values(), hide_method_values() (+6 more)

### Community 9 - "cli.py"
Cohesion: 0.05
Nodes (37): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+29 more)

### Community 10 - "intercept.c"
Cohesion: 0.20
Nodes (23): classify_path(), decode_evdev_ioctl(), decode_hidraw_ioctl(), decode_ioctl(), dev_type_str(), dup(), dup2(), dup3() (+15 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.08
Nodes (27): check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script, status_field(), unit_prop(), test_daemon_ctl.sh script, bad_utf8_rejected() (+19 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.05
Nodes (21): DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name, m_ds4NameIsDefault, m_dualsenseBackend (+13 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.11
Nodes (30): button_names_joined(), ButtonSpec, bit, dpad, name, CommandSpec, arg_completions, name (+22 more)

### Community 14 - "TEST_F"
Cohesion: 0.10
Nodes (16): abi_matches(), copy_field(), driver_transport_attach(), driver_transport_available(), driver_transport_claim(), driver_transport_create_virtual(), driver_transport_detach(), driver_transport_link_virtual() (+8 more)

### Community 15 - "UI IPC client"
Cohesion: 0.09
Nodes (16): LLVMFuzzerTestOneInput(), IpcClient::closeConnection(), IpcClient::isBusy(), IpcClient::onReadable(), IpcClient::onTimeout(), IpcClient::startNext(), IpcClient, IpcClient::IpcClient() (+8 more)

### Community 16 - "libudev stub tests"
Cohesion: 0.09
Nodes (14): fake_parent(), is_tagged(), physical_ds4(), TEST(), udev_device, devpath, pid, uevent (+6 more)

### Community 17 - "ds4t-chardev.c"
Cohesion: 0.13
Nodes (13): ds4t_dbg_show(), ds4t_feature_request(), ds4t_get_calibration(), ds4t_ioctl(), ds4t_pad_find(), ds4t_pad_get(), ds4t_pad_report(), ds4t_poll() (+5 more)

### Community 18 - "HID-BPF programs"
Cohesion: 0.11
Nodes (8): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG()

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.09
Nodes (21): abort_transport(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx, device_address, ep_in (+13 more)

### Community 20 - "functionfs-backend.c"
Cohesion: 0.12
Nodes (19): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_event_packet() (+11 more)

### Community 21 - "ipc-client.cpp"
Cohesion: 0.17
Nodes (25): build_led(), build_set_mac_squat(), connect_socket(), decode_buttons(), dpad_name(), errno_text(), format_state_line(), format_timing_line() (+17 more)

### Community 22 - "scenario_from"
Cohesion: 0.17
Nodes (3): scenario_from(), ParsingTest, VariablesTest

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.07
Nodes (23): out_loop(), BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_ (+15 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "FunctionFSDevice"
Cohesion: 0.08
Nodes (22): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+14 more)

### Community 27 - "TrayIcon"
Cohesion: 0.13
Nodes (11): AppController, DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction, m_tray (+3 more)

### Community 28 - "hid-bpf-transport.cpp"
Cohesion: 0.14
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), on_ringbuf_report(), run_udev_hid_bpf() (+8 more)

### Community 29 - "ioctl"
Cohesion: 0.19
Nodes (19): pump_thread_fn(), read_hid_ignore_special_drivers(), pump_thread_fn(), do_calibration(), do_control(), do_feature_get(), do_feature_set(), do_hidraw_feature() (+11 more)

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.15
Nodes (12): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), DaemonController::backendValues(), DaemonController::DaemonController(), DaemonController::hideMethodValues() (+4 more)

### Community 32 - "steps"
Cohesion: 0.09
Nodes (22): runs-on, steps, jobs, analyze, name, on, pull_request, push (+14 more)

### Community 33 - "helpers.py"
Cohesion: 0.22
Nodes (3): CliTest, run_cli(), scenario_file()

### Community 34 - "TEST_F"
Cohesion: 0.14
Nodes (3): usb_hid_transport_start(), TEST_F(), UsbTransportTest

### Community 35 - "lint.yml"
Cohesion: 0.12
Nodes (22): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+14 more)

### Community 36 - "InputState"
Cohesion: 0.10
Nodes (20): decode_touch_point(), InputState, accel, buttons, dpad, gyro, has_motion, has_touch (+12 more)

### Community 37 - "trayicon.cpp"
Cohesion: 0.15
Nodes (3): appletIcon(), TrayIcon::buildMenu(), TrayIcon::TrayIcon()

### Community 38 - "TEST_F"
Cohesion: 0.15
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "libusb_stub.cpp"
Cohesion: 0.19
Nodes (16): cancel_active_transfer(), usb_hid_transport_close(), Clock, libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_detach_kernel_driver(), libusb_device_handle (+8 more)

### Community 40 - "inputmonitor.cpp"
Cohesion: 0.18
Nodes (7): InputMonitor::clearStates(), InputMonitor::dpadName(), InputMonitor::measure(), InputMonitor::note(), InputMonitor::onReadable(), InputMonitor::tryConnect(), InputMonitor::InputMonitor()

### Community 41 - "dualsense_input_report"
Cohesion: 0.11
Nodes (18): dualsense_input_report, accel, buttons, gyro, points, reserved, reserved2, reserved3 (+10 more)

### Community 42 - "test_driver_transport.cpp"
Cohesion: 0.28
Nodes (3): ns_per_op(), report(), TEST()

### Community 43 - "ds4t-virtual.c"
Cohesion: 0.13
Nodes (20): ds4_calibration_report(), ds4t_parse_mac(), ds4t_virt_alloc(), ds4t_virt_build_pad_output(), ds4t_virt_emit(), ds4t_virt_feature(), ds4t_virt_ll_output_report(), ds4t_virt_ll_parse() (+12 more)

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "gtest"
Cohesion: 0.14
Nodes (7): parse(), Parsed, bus, ok, product, vendor, TEST()

### Community 47 - "usb_hid_transport_open"
Cohesion: 0.23
Nodes (14): device_still_present(), is_own_virtual_usb_device(), usb_hid_transport_open(), libusb_context, libusb_device, libusb_exit(), libusb_free_device_list(), libusb_get_bus_number() (+6 more)

### Community 49 - "close"
Cohesion: 0.19
Nodes (6): FakeDaemon, dir_, handler_, listen_fd_, thread_, read_command()

### Community 50 - "src/main.cpp"
Cohesion: 0.11
Nodes (29): device_node_sysfs_path(), driver_push_ignore_mac(), find_physical_ds4(), find_usb_ds4_hid_id(), get_event_nodes(), hidraw_to_hid_id(), is_own_virtual_hidraw(), kernel_link_virtual() (+21 more)

### Community 51 - "Button"
Cohesion: 0.13
Nodes (15): Button, kBtnCircle, kBtnCross, kBtnL1, kBtnL2, kBtnL3, kBtnOptions, kBtnPs (+7 more)

### Community 52 - "dualshock4_input_report_common"
Cohesion: 0.13
Nodes (15): dualshock4_input_report_common, accel, buttons, gyro, reserved2, reserved3, rx, ry (+7 more)

### Community 53 - "Button"
Cohesion: 0.13
Nodes (15): Button, Circle, Cross, L1, L2, L3, Options, Ps (+7 more)

### Community 54 - "tests.yml"
Cohesion: 0.15
Nodes (12): concurrency, cancel-in-progress, group, name, on, pull_request, push, workflow_call (+4 more)

### Community 55 - "name"
Cohesion: 0.24
Nodes (19): steps, steps, steps, steps, steps, steps, steps, continue-on-error (+11 more)

### Community 56 - "DualShock 4 base layout diagram (unlabelled SVG)"
Cohesion: 0.17
Nodes (12): Shoulder bumpers L1/R1, DualShock 4 controller, DualShock 4 base layout diagram (unlabelled SVG), D-pad, Face buttons (cross, circle, square, triangle), Light bar, PS button, Share and Options buttons (+4 more)

### Community 57 - "TEST"
Cohesion: 0.33
Nodes (10): ds4t_build_ds4(), ds4t_build_dualsense(), ds4t_calibration_to_usb_layout(), ds4t_crc32(), ds4t_decode_ds4_battery(), ds4t_encode_dualsense_battery(), ds4t_parse_input(), ds4t_parse_output() (+2 more)

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): jobs, performance, security-static, smoke-sanity, unit, needs, runs-on, needs (+4 more)

### Community 60 - "test_driver_transport.cpp"
Cohesion: 0.14
Nodes (6): Clock, DriverTransportTest, dev_dir_, module_dir_, root_, Clock

### Community 61 - "unistd"
Cohesion: 0.15
Nodes (6): set_error(), write_all(), write_file_atomic(), SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance::SingleInstance()

### Community 62 - "Security findings register"
Cohesion: 0.20
Nodes (10): H2 control socket access policy (SO_PEERCRED, ds4-translator group), L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, ds4-ctl runtime control utility, ds4-translator daemon, Test suite (make test targets), ds4-translator-ui Qt target (+2 more)

### Community 63 - "ds4t-driver.c"
Cohesion: 0.12
Nodes (15): ds4t_pad_alloc(), ds4t_pad_register(), ds4t_mac_ignored(), ds4t_attach(), ds4t_attached(), ds4t_bus_event(), ds4t_is_emulated(), ds4t_probe() (+7 more)

### Community 64 - "Hide methods (legacy/unbind/unlink)"
Cohesion: 0.27
Nodes (10): hid-generic swap, Hide methods (legacy/unbind/unlink), MAC squat, Unlink of /dev nodes and reapply sweep, Workaround stack the module would replace, Physical controller hiding, hid-bpf-probe feasibility spike, HID-BPF hid_device_event swallow and ringbuf mirror (+2 more)

### Community 66 - "TimingStats"
Cohesion: 0.20
Nodes (9): TimingStats, interval_max_us, interval_mean_us, interval_min_us, latency_max_us, latency_mean_us, reports, window_us (+1 more)

### Community 67 - "udev-spoof.c"
Cohesion: 0.31
Nodes (7): init_functions(), is_virtual_sony_controller(), next_uevent_buffer(), udev_device_get_parent_with_subsystem_devtype(), udev_device_get_subsystem(), udev_device_get_sysattr_value(), virtual_mac_serial()

### Community 68 - "steps"
Cohesion: 0.22
Nodes (8): description, name, runs, steps, using, name, run, shell

### Community 70 - "compatibility"
Cohesion: 0.25
Nodes (8): needs, runs-on, strategy, compatibility, compiler, os, fail-fast, matrix

### Community 71 - "ds4-translator README"
Cohesion: 0.20
Nodes (12): pacman repository landing page, arch-repo release CI job and GitHub Pages deploy, Arch pacman repository setup, Package signing with release subkey, Backend selection (uhid vs functionfs), Known issues (Steam MAC, BT auth popup), ds4-translator README, USB priority live hot-swap (+4 more)

### Community 72 - "OutputState"
Cohesion: 0.25
Nodes (8): format_output_line(), OutputState, blue, green, red, rumble_left, rumble_right, parse_output()

### Community 73 - "How DS4-Translator Works"
Cohesion: 0.33
Nodes (7): FunctionFS backend, How DS4-Translator Works, Haptics rumble heuristic, Emulation type none, udev hide rules (72-ds4-translator-hide.rules), Virtual controller identity and feature reports, Light bar flash passthrough

### Community 74 - "HideMethod"
Cohesion: 0.28
Nodes (9): hide_method_config_str(), hide_method_from_str(), hide_method_is_unbind_family(), HideMethod, HIDE_METHOD_DRIVER, HIDE_METHOD_LEGACY, HIDE_METHOD_UNBIND, HIDE_METHOD_UNLINK (+1 more)

### Community 75 - "hid-ds4-translator kernel module (Option A binder)"
Cohesion: 0.29
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 78 - "BtSuppression"
Cohesion: 0.18
Nodes (5): BtSuppression, Ops, exists, usb_present, TEST()

### Community 79 - "Bluetooth HID authorization prompt screenshot"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 80 - "rules"
Cohesion: 0.33
Nodes (5): rules, self-repository, superfluous-actions, disable, ignore

### Community 81 - "dualsense_touch_point"
Cohesion: 0.33
Nodes (6): dualsense_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 82 - "ds4t-control.c"
Cohesion: 0.14
Nodes (8): ds4t_control_create_virtual(), ds4t_control_device(), ds4t_control_inject(), ds4t_control_ioctl(), ds4t_control_link_virtual(), ds4t_control_set_ignore_mac(), ds4t_virt_hid(), ds4t_virtual_create()

### Community 83 - "hid-unbind-detect.cpp"
Cohesion: 0.36
Nodes (10): hid_bus_scan_existing(), hid_devpath_is_own_virtual(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+2 more)

### Community 85 - "compatibility-arch"
Cohesion: 0.50
Nodes (4): container, needs, runs-on, compatibility-arch

### Community 86 - "dualshock4_touch_point"
Cohesion: 0.33
Nodes (6): dualshock4_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 88 - "Source"
Cohesion: 0.50
Nodes (3): Source, Physical, Virtual

### Community 90 - "security"
Cohesion: 0.67
Nodes (3): security, needs, runs-on

### Community 92 - "test-virtual.sh"
Cohesion: 0.08
Nodes (24): run-suites.sh script, attach(), detach(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, test-binder.sh script, cleanup_hotswap() (+16 more)

### Community 94 - "ds4t_pad_put"
Cohesion: 0.29
Nodes (7): ds4t_pad_put(), ds4t_pad_unregister(), ds4t_release(), ds4t_control_release(), ds4t_detach(), ds4t_remove(), ds4t_virtual_destroy()

### Community 95 - "vector"
Cohesion: 0.21
Nodes (5): reapply_unlinked_nodes(), ds4_report(), ds5_report(), parse(), TEST()

### Community 101 - "steps"
Cohesion: 0.10
Nodes (19): container, runs-on, steps, jobs, build, name, on, pull_request (+11 more)

### Community 102 - "dualshock4_input_report_bt"
Cohesion: 0.25
Nodes (8): dualshock4_input_report_bt, common, crc32, num_touch_reports, report_id, reserved, reserved2, touch_reports

### Community 103 - "interrupt_transfer"
Cohesion: 0.36
Nodes (6): interrupt_transfer(), transfer_status_to_error(), libusb_alloc_transfer(), libusb_cancel_transfer(), libusb_free_transfer(), libusb_submit_transfer()

### Community 104 - "ControllerType"
Cohesion: 0.30
Nodes (11): driver_transport_inject(), functionfs_send_input_report(), emit_input_report(), build_ds4_usb_report(), build_dualsense_report(), build_gip_input_report(), build_xbox360_report(), controller_type_is_gip() (+3 more)

### Community 105 - "integration"
Cohesion: 0.67
Nodes (3): needs, runs-on, integration

### Community 106 - "dualshock4_input_report_usb"
Cohesion: 0.18
Nodes (9): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports, dualshock4_touch_report, points (+1 more)

### Community 107 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 112 - "PhysicalNode"
Cohesion: 0.13
Nodes (11): open_and_hide_physical(), PhysicalNode, fd, is_grabbed, is_unlinked, orig_gid, orig_mode, orig_uid (+3 more)

### Community 113 - "broker.py"
Cohesion: 0.06
Nodes (18): RootBrokerTest, serve_lines(), ServeTest, StreamProcessTest, connect(), fuzz(), idle(), raw() (+10 more)

### Community 114 - "transport-check.cpp"
Cohesion: 0.53
Nodes (4): main(), read_reports(), wait_for_disconnect(), write_output()

### Community 119 - "ds4t_init"
Cohesion: 0.33
Nodes (6): ds4t_debugfs_exit(), ds4t_debugfs_init(), ds4t_control_exit(), ds4t_control_init(), ds4t_exit(), ds4t_init()

### Community 122 - "hid-ds4-translator (kernel module)"
Cohesion: 0.29
Nodes (7): Build, hid-ds4-translator (kernel module), Install with DKMS, Lint and static analysis, Secure Boot, Test without hardware, Try it

### Community 123 - "paths.h"
Cohesion: 0.53
Nodes (8): config_path(), driver_dev_dir(), driver_module_dir(), from_env(), none_sentinel_path(), runtime_dir(), socket_path(), state_dir()

### Community 124 - "string"
Cohesion: 0.18
Nodes (5): StatusField, key, value, LLVMFuzzerTestOneInput(), sample_state()

### Community 125 - "ioctl"
Cohesion: 0.24
Nodes (9): abs_value(), fill_injected(), hidraw_rumble(), main(), open_event_node(), print_state(), rumble(), sleep_ms() (+1 more)

### Community 126 - "close"
Cohesion: 0.50
Nodes (5): bind, hid_uevent_monitor_open(), background_delayed_udevadm_trigger(), setup_ipc_socket(), close()

### Community 127 - "test_kernel_translate.cpp"
Cohesion: 0.14
Nodes (12): calculate_crc32(), ds4_calibration_to_usb_layout(), bt_report(), daemon_parse_bt(), daemon_parse_usb(), DaemonView, common, num_touch (+4 more)

### Community 128 - "test_steps.py"
Cohesion: 0.31
Nodes (3): GuardTest, PlanTest, status()

### Community 129 - "lib.sh"
Cohesion: 0.21
Nodes (6): bound_driver(), check(), hid_dev_by_mac(), nok(), ok(), lib.sh script

### Community 130 - "SyntheticState"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

### Community 131 - "FakeBus"
Cohesion: 0.16
Nodes (5): FakeBus, bind_attempts, bt_bound, bt_exists, usb

### Community 132 - "gtest"
Cohesion: 0.33
Nodes (4): bluetooth_calibration_report(), fill_payload(), le16(), neutral()

### Community 133 - "SingleInstance"
Cohesion: 0.19
Nodes (7): main(), SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance, m_server, public, TrayIcon::isAvailable()

### Community 135 - "fake-pad.c"
Cohesion: 0.22
Nodes (9): answer_get_report(), crc32_seeded(), create_device(), handle_events(), main(), send_input(), sleep_ms(), uhid_write() (+1 more)

### Community 136 - "Controls"
Cohesion: 0.33
Nodes (5): Controls, feed, gone, reclaimed, wedge

### Community 137 - "closedir"
Cohesion: 0.38
Nodes (10): find_udc(), hid_id_by_mac(), open_hidraw_by_mac(), closedir(), get_tracked_dir(), is_interesting_dir(), opendir(), readdir() (+2 more)

### Community 138 - "BatteryState"
Cohesion: 0.33
Nodes (6): BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL, decode_ds4_battery(), encode_dualsense_battery()

### Community 142 - "find_hid_interface"
Cohesion: 0.67
Nodes (3): find_hid_interface(), libusb_free_config_descriptor(), libusb_get_active_config_descriptor()

### Community 143 - "type_values"
Cohesion: 0.67
Nodes (3): build_set_type(), is_valid_type(), type_values()

### Community 146 - "load_physical_calibration"
Cohesion: 0.40
Nodes (4): driver_transport_get_calibration(), functionfs_set_calibration(), load_physical_calibration(), set_physical_calibration()

### Community 147 - "ReadStatus"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

## Knowledge Gaps
- **368 isolated node(s):** `fd`, `orig_mode`, `is_grabbed`, `is_unlinked`, `rdev` (+363 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 980 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **40 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `inputmonitor.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.072) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `fd`, `orig_mode`, `is_grabbed` to the rest of the system?**
  _368 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._
- **Why does `DaemonController` connect `DaemonController (UI)` to `UI IPC client`, `QString`, `trayicon.cpp`, `daemoncontroller.cpp`?**
  _High betweenness centrality (0.066) - this node is a cross-community bridge._
- **Are the 22 inferred relationships involving `main()` (e.g. with `.pending()` and `.poll()`) actually correct?**
  _`main()` has 22 INFERRED edges - model-reasoned connections that need verification._
- **Should `main` be split into smaller, more focused modules?**
  _Cohesion score 0.08846153846153847 - nodes in this community are weakly interconnected._