# Graph Report - ds4-translator  (2026-11-27)

## Corpus Check
- 150 files · ~168,321 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 40 file(s) not represented in the graph (top: .qml 15, (none) 8, .toml 5)
- WARNING: 40 of 195 files seen (21%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 2705 nodes · 5284 edges · 171 communities (124 shown, 47 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 617 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `21e686c1`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Result
- Qt UI model (DS4 settings)
- PhysicalFeatureIo
- Translation unit tests
- TEST
- Release workflow (CI)
- ds4t-gadget.c
- TEST_F
- ControllerType
- Executor
- intercept.c
- Hardware verify.sh
- DaemonController (UI)
- ds4-ctl CLI
- test_pairing.cpp
- UI IPC client
- libudev stub tests
- src/main.cpp
- hid-bpf/hid_bpf_helpers.h
- USB HID transport (libusb)
- ds4t-vcore.c
- ipc-client.cpp
- TEST
- Bluetooth output pacer
- AppController (UI)
- FunctionFSDevice
- TrayIcon
- hid-unbind-detect.cpp
- TEST_F
- Console
- daemoncontroller.cpp
- steps
- translate.cpp
- TEST_F
- lint.yml
- InputState
- trayicon.cpp
- TEST_F
- interrupt_transfer
- inputmonitor.cpp
- dualsense_input_report
- functionfs-backend.c
- scenario_from
- QString
- DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)
- helpers.py
- usb_hid_transport_open
- hid-bpf-transport.cpp
- plan
- appcontroller.cpp
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- test_translate.cpp
- TEST
- jobs
- ds4t-vhid.c
- libusb_stub.cpp
- Security findings register
- main
- Hide methods (legacy/unbind/unlink)
- TimingStats
- steps
- IncludeTest
- compatibility
- ds4-translator README
- OutputState
- How DS4-Translator Works
- Controls
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- dualshock4_touch_point
- QString
- Bluetooth HID authorization prompt screenshot
- TEST
- ds4t-chardev.c
- udev-spoof.c
- time
- static_checks.sh
- compatibility-arch
- ds4t-driver.c
- bisect-capabilities.sh
- Source
- ds4-ctl-completion.bash
- security
- TEST
- CalibrationController
- simulate_arch_release.sh
- virtual-check.c
- ioctl
- rebind-unbound-controllers.sh
- bt-disconnect.sh
- build.sh
- InputMonitor::disconnectStream
- Graphify usage rules
- unistd
- SingleInstance
- paths.h
- string
- dualshock4_input_report_bt
- dualsense_touch_point
- dualshock4_input_report_usb
- Debugging and Interceptor Guide
- TEST
- StickCoverage
- test-hotswap.sh
- ds4t-translate.c
- translate.h
- ds4t-control.c
- test_ipc_transport.cpp
- TEST
- lib.sh
- scenario.py
- TEST
- unistd
- string
- closedir
- test-binder.sh
- test-gadget.sh
- test-virtual.sh
- DriverTransportTest
- hid-ds4-translator (kernel module)
- rules
- read_available
- calibrationcontroller.h
- TEST
- integration
- analyze.sh
- run-vm.sh
- test-faults.sh
- broker.py
- cli.py
- steps.py
- kernel.yml
- vector
- TEST
- RootBroker
- calibrationcontroller.cpp
- fake-pad.c
- Action
- fs-util.cpp
- hid-driver-sysfs.h
- TouchPoint
- BatteryState

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 90 edges
3. `TEST()` - 82 edges
4. `DaemonController` - 73 edges
5. `main()` - 68 edges
6. `CalibrationController` - 67 edges
7. `close()` - 47 edges
8. `AppController` - 38 edges
9. `ioctl()` - 37 edges
10. `TransportState` - 34 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `LLVMFuzzerTestOneInput()` --calls--> `parse_hid_id()`  [INFERRED]
  tests/fuzz/fuzz_parsers.cpp → src/hid-unbind-detect.cpp
- `in_loop()` --calls--> `write()`  [INFERRED]
  src/functionfs-backend.c → tools/ds4-intercept/intercept.c
- `on_ringbuf_report()` --calls--> `write()`  [INFERRED]
  src/hid-bpf-transport.cpp → tools/ds4-intercept/intercept.c
- `run_virtual_ui()` --calls--> `read()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (171 total, 47 thin omitted)

### Community 0 - "Result"
Cohesion: 0.10
Nodes (11): FakeRunner, FakeUI, GuardTest, PromptTest, RepeatTest, RestoreTest, run_scenario(), RunStepTest (+3 more)

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "PhysicalFeatureIo"
Cohesion: 0.24
Nodes (7): PhysicalFeatureIo, fd_, via_usb_transport_, feature_control_transfer(), find_transport(), usb_hid_transport_get_feature_report(), usb_hid_transport_set_feature_report()

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (47): container, needs, permissions, runs-on, steps, needs, permissions, runs-on (+39 more)

### Community 6 - "ds4t-gadget.c"
Cohesion: 0.11
Nodes (27): ds4t_gadget_alloc_req(), ds4t_gadget_bind(), ds4t_gadget_class(), ds4t_gadget_config_desc(), ds4t_gadget_disable(), ds4t_gadget_disconnect(), ds4t_gadget_enable(), ds4t_gadget_exit() (+19 more)

### Community 7 - "TEST_F"
Cohesion: 0.11
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "ControllerType"
Cohesion: 0.09
Nodes (30): functionfs_set_mac(), backend_for_type(), backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_KERNEL, BACKEND_KERNEL_USB (+22 more)

### Community 9 - "Executor"
Cohesion: 0.14
Nodes (9): Step, substitute(), Executor, hit(), probe(), Outcome, Runner, StepFailed (+1 more)

### Community 10 - "intercept.c"
Cohesion: 0.15
Nodes (33): pump_thread_fn(), background_delayed_udevadm_trigger(), read_hid_ignore_special_drivers(), write_hid_ignore_special_drivers(), pump_thread_fn(), classify_path(), close(), decode_evdev_ioctl() (+25 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.07
Nodes (32): expect(), expect_includes(), run_cases(), test_completion.sh script, check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script (+24 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.06
Nodes (21): DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name, m_ds4NameIsDefault, m_dualsenseBackend (+13 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.11
Nodes (37): button_names_joined(), ButtonSpec, bit, dpad, name, calibrate_step(), calibrate_stick_center(), calibrate_triggers() (+29 more)

### Community 14 - "test_pairing.cpp"
Cohesion: 0.13
Nodes (5): CablePairTest, dir, key, pad, TEST_F()

### Community 15 - "UI IPC client"
Cohesion: 0.09
Nodes (16): LLVMFuzzerTestOneInput(), IpcClient::closeConnection(), IpcClient::isBusy(), IpcClient::onReadable(), IpcClient::onTimeout(), IpcClient::startNext(), IpcClient, IpcClient::IpcClient() (+8 more)

### Community 16 - "libudev stub tests"
Cohesion: 0.09
Nodes (14): fake_parent(), is_tagged(), physical_ds4(), TEST(), udev_device, devpath, pid, uevent (+6 more)

### Community 17 - "src/main.cpp"
Cohesion: 0.06
Nodes (56): driver_transport_get_calibration(), driver_transport_link_virtual(), functionfs_set_calibration(), functionfs_set_ds4_firmware(), capture_physical_features_hidraw(), capture_physical_features_usb(), clear_physical_firmware(), device_node_sysfs_path() (+48 more)

### Community 18 - "hid-bpf/hid_bpf_helpers.h"
Cohesion: 0.07
Nodes (17): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), bluez_ds4_device_info(), build_ds4_pairing_report(), cable_pair(), choose_adapter() (+9 more)

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.09
Nodes (21): abort_transport(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx, device_address, ep_in (+13 more)

### Community 20 - "ds4t-vcore.c"
Cohesion: 0.10
Nodes (35): ds4t_pad_find(), ds4t_pad_get(), ds4t_pad_put(), ds4t_pad_unregister(), ds4t_release(), ds4t_remove(), ds4t_gadget_ep0_complete(), ds4_calibration_report() (+27 more)

### Community 21 - "ipc-client.cpp"
Cohesion: 0.17
Nodes (26): build_led(), build_set_hide_method(), build_set_name(), connect_socket(), decode_buttons(), dpad_name(), errno_text(), format_state_line() (+18 more)

### Community 22 - "TEST"
Cohesion: 0.06
Nodes (39): build_calibration_command(), build_flash_mode_report(), build_flash_select_report(), build_reset_report(), calibration_run_step(), calibration_target_name(), CalibrationStep, CAL_STEP_SAMPLE (+31 more)

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.09
Nodes (18): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+10 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "FunctionFSDevice"
Cohesion: 0.08
Nodes (22): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+14 more)

### Community 27 - "TrayIcon"
Cohesion: 0.13
Nodes (11): AppController, DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction, m_tray (+3 more)

### Community 28 - "hid-unbind-detect.cpp"
Cohesion: 0.36
Nodes (10): hid_bus_scan_existing(), hid_devpath_is_own_virtual(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+2 more)

### Community 29 - "TEST_F"
Cohesion: 0.10
Nodes (16): abi_matches(), copy_field(), driver_transport_attach(), driver_transport_available(), driver_transport_claim(), driver_transport_create_virtual(), driver_transport_detach(), driver_transport_module_instance() (+8 more)

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.17
Nodes (12): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), DaemonController::backendValues(), DaemonController::DaemonController(), DaemonController::hideMethodValues() (+4 more)

### Community 32 - "steps"
Cohesion: 0.09
Nodes (22): runs-on, steps, jobs, analyze, name, on, pull_request, push (+14 more)

### Community 33 - "translate.cpp"
Cohesion: 0.15
Nodes (24): driver_transport_inject(), functionfs_send_input_report(), emit_input_report(), ascii_field(), build_ds4_usb_report(), build_dualsense_report(), build_gip_input_report(), build_xbox360_report() (+16 more)

### Community 34 - "TEST_F"
Cohesion: 0.12
Nodes (3): usb_hid_transport_start(), TEST_F(), UsbTransportTest

### Community 35 - "lint.yml"
Cohesion: 0.12
Nodes (22): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+14 more)

### Community 36 - "InputState"
Cohesion: 0.12
Nodes (16): InputState, accel, buttons, dpad, gyro, has_motion, has_touch, l2 (+8 more)

### Community 38 - "TEST_F"
Cohesion: 0.15
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "interrupt_transfer"
Cohesion: 0.31
Nodes (7): cancel_active_transfer(), interrupt_transfer(), transfer_status_to_error(), libusb_alloc_transfer(), libusb_cancel_transfer(), libusb_free_transfer(), libusb_submit_transfer()

### Community 40 - "inputmonitor.cpp"
Cohesion: 0.18
Nodes (7): InputMonitor::clearStates(), InputMonitor::dpadName(), InputMonitor::measure(), InputMonitor::note(), InputMonitor::onReadable(), InputMonitor::tryConnect(), InputMonitor::InputMonitor()

### Community 41 - "dualsense_input_report"
Cohesion: 0.11
Nodes (18): dualsense_input_report, accel, buttons, gyro, points, reserved, reserved2, reserved3 (+10 more)

### Community 42 - "functionfs-backend.c"
Cohesion: 0.16
Nodes (18): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_event_packet() (+10 more)

### Community 43 - "scenario_from"
Cohesion: 0.14
Nodes (4): scenario_from(), ParsingTest, VariablesTest, count_steps()

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "helpers.py"
Cohesion: 0.24
Nodes (3): CliTest, run_cli(), scenario_file()

### Community 47 - "usb_hid_transport_open"
Cohesion: 0.23
Nodes (11): device_still_present(), find_hid_interface(), is_own_virtual_usb_device(), libusb_device, libusb_free_config_descriptor(), libusb_free_device_list(), libusb_get_active_config_descriptor(), libusb_get_bus_number() (+3 more)

### Community 48 - "hid-bpf-transport.cpp"
Cohesion: 0.13
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), on_ringbuf_report(), run_udev_hid_bpf() (+8 more)

### Community 49 - "plan"
Cohesion: 0.22
Nodes (4): PlanTest, _Lenient, plan(), walk()

### Community 50 - "appcontroller.cpp"
Cohesion: 0.12
Nodes (5): AppController::AppController(), AppController::autostartFilePath(), SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance::SingleInstance()

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

### Community 57 - "test_translate.cpp"
Cohesion: 0.10
Nodes (17): build_ds4_output_report(), calculate_crc32(), Ds4Output, b, flash_off, flash_on, g, motor_left (+9 more)

### Community 58 - "TEST"
Cohesion: 0.14
Nodes (7): parse(), Parsed, bus, ok, product, vendor, TEST()

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): jobs, performance, security-static, smoke-sanity, unit, needs, runs-on, needs (+4 more)

### Community 60 - "ds4t-vhid.c"
Cohesion: 0.19
Nodes (7): ds4t_control_device(), ds4t_vhid_core(), ds4t_vhid_front_input(), ds4t_vhid_front_start(), ds4t_vhid_output_report(), ds4t_vhid_parse(), ds4t_vhid_raw_request()

### Community 61 - "libusb_stub.cpp"
Cohesion: 0.17
Nodes (23): usb_hid_transport_close(), usb_hid_transport_open(), Clock, last_feature_set(), libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_context (+15 more)

### Community 62 - "Security findings register"
Cohesion: 0.20
Nodes (10): H2 control socket access policy (SO_PEERCRED, ds4-translator group), L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, ds4-ctl runtime control utility, ds4-translator daemon, Test suite (make test targets), ds4-translator-ui Qt target (+2 more)

### Community 63 - "main"
Cohesion: 0.08
Nodes (34): bind, driver_transport_destroy_virtual(), hid_uevent_monitor_open(), build_synthetic_common(), destroy_virtual_device(), emit_neutral_report(), flush_bt_output_for_fd(), hide_method_config_str() (+26 more)

### Community 64 - "Hide methods (legacy/unbind/unlink)"
Cohesion: 0.27
Nodes (10): hid-generic swap, Hide methods (legacy/unbind/unlink), MAC squat, Unlink of /dev nodes and reapply sweep, Workaround stack the module would replace, Physical controller hiding, hid-bpf-probe feasibility spike, HID-BPF hid_device_event swallow and ringbuf mirror (+2 more)

### Community 66 - "TimingStats"
Cohesion: 0.20
Nodes (9): TimingStats, interval_max_us, interval_mean_us, interval_min_us, latency_max_us, latency_mean_us, reports, window_us (+1 more)

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

### Community 74 - "Controls"
Cohesion: 0.29
Nodes (5): Controls, feed, gone, reclaimed, wedge

### Community 75 - "hid-ds4-translator kernel module (Option A binder)"
Cohesion: 0.29
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "dualshock4_touch_point"
Cohesion: 0.33
Nodes (6): dualshock4_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 79 - "Bluetooth HID authorization prompt screenshot"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 81 - "ds4t-chardev.c"
Cohesion: 0.14
Nodes (11): ds4t_dbg_show(), ds4t_feature_request(), ds4t_get_calibration(), ds4t_ioctl(), ds4t_pad_report(), ds4t_poll(), ds4t_read(), ds4t_ring_at() (+3 more)

### Community 82 - "udev-spoof.c"
Cohesion: 0.27
Nodes (7): init_functions(), is_virtual_sony_controller(), next_uevent_buffer(), udev_device_get_parent_with_subsystem_devtype(), udev_device_get_subsystem(), udev_device_get_sysattr_value(), virtual_mac_serial()

### Community 83 - "time"
Cohesion: 0.15
Nodes (12): buzz_evdev(), buzz_hidraw(), main(), read_text(), steam_pad(), sysfs_nodes(), connect(), fuzz() (+4 more)

### Community 85 - "compatibility-arch"
Cohesion: 0.50
Nodes (4): container, needs, runs-on, compatibility-arch

### Community 86 - "ds4t-driver.c"
Cohesion: 0.13
Nodes (17): ds4t_pad_alloc(), ds4t_pad_register(), ds4t_control_release(), ds4t_mac_ignored(), ds4t_attach(), ds4t_attached(), ds4t_bus_event(), ds4t_detach() (+9 more)

### Community 88 - "Source"
Cohesion: 0.50
Nodes (3): Source, Physical, Virtual

### Community 90 - "security"
Cohesion: 0.67
Nodes (3): security, needs, runs-on

### Community 91 - "TEST"
Cohesion: 0.15
Nodes (10): bt_report(), daemon_parse_bt(), daemon_parse_usb(), DaemonView, common, num_touch, touch, expect_same() (+2 more)

### Community 92 - "CalibrationController"
Cohesion: 0.05
Nodes (22): CalibrationController, m_actions, m_coverage, m_instruction, m_ipc, m_keepRotatingHint, m_message, m_pauseTimer (+14 more)

### Community 94 - "virtual-check.c"
Cohesion: 0.19
Nodes (14): hidraw_get_feature(), abs_value(), fill_injected(), get_fail_nth(), hidraw_rumble(), main(), open_event_node(), print_state() (+6 more)

### Community 95 - "ioctl"
Cohesion: 0.31
Nodes (12): do_calibration(), do_control(), do_feature_get(), do_feature_set(), do_hidraw_feature(), do_hold(), do_info(), do_read() (+4 more)

### Community 101 - "unistd"
Cohesion: 0.14
Nodes (5): Clock, FakePad, accepts_set, answers, sent

### Community 102 - "SingleInstance"
Cohesion: 0.17
Nodes (7): main(), SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance, m_server, public, TrayIcon::isAvailable()

### Community 103 - "paths.h"
Cohesion: 0.51
Nodes (8): config_path(), driver_dev_dir(), driver_module_dir(), from_env(), none_sentinel_path(), runtime_dir(), socket_path(), state_dir()

### Community 104 - "string"
Cohesion: 0.25
Nodes (4): StatusField, key, value, LLVMFuzzerTestOneInput()

### Community 105 - "dualshock4_input_report_bt"
Cohesion: 0.25
Nodes (8): dualshock4_input_report_bt, common, crc32, num_touch_reports, report_id, reserved, reserved2, touch_reports

### Community 106 - "dualsense_touch_point"
Cohesion: 0.33
Nodes (6): dualsense_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 107 - "dualshock4_input_report_usb"
Cohesion: 0.33
Nodes (6): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports

### Community 108 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 109 - "TEST"
Cohesion: 0.16
Nodes (5): FakeBus, bind_attempts, bt_bound, bt_exists, usb

### Community 112 - "StickCoverage"
Cohesion: 0.11
Nodes (15): StickCoverage::progress(), StickCoverage, kBuckets, kBucketsForCircle, kMinRadius, kTargetCircles, m_sticks, State (+7 more)

### Community 114 - "test-hotswap.sh"
Cohesion: 0.21
Nodes (11): cleanup_hotswap(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, DS4_TEST_UHID_USB_IS_PHYSICAL, phys_binds(), scenario(), test-hotswap.sh script (+3 more)

### Community 115 - "ds4t-translate.c"
Cohesion: 0.33
Nodes (10): ds4t_build_ds4(), ds4t_build_dualsense(), ds4t_calibration_to_usb_layout(), ds4t_crc32(), ds4t_decode_ds4_battery(), ds4t_encode_dualsense_battery(), ds4t_parse_input(), ds4t_parse_output() (+2 more)

### Community 116 - "translate.h"
Cohesion: 0.23
Nodes (3): dualshock4_touch_report, points, timestamp

### Community 117 - "ds4t-control.c"
Cohesion: 0.18
Nodes (11): ds4t_debugfs_exit(), ds4t_debugfs_init(), ds4t_control_create_virtual(), ds4t_control_exit(), ds4t_control_init(), ds4t_control_inject(), ds4t_control_ioctl(), ds4t_control_link_virtual() (+3 more)

### Community 118 - "test_ipc_transport.cpp"
Cohesion: 0.22
Nodes (6): FakeDaemon, dir_, handler_, listen_fd_, thread_, read_command()

### Community 119 - "TEST"
Cohesion: 0.28
Nodes (3): ns_per_op(), report(), TEST()

### Community 120 - "lib.sh"
Cohesion: 0.21
Nodes (9): bound_driver(), check(), check_no_splat(), cleanup(), hid_dev_by_mac(), nok(), ok(), lib.sh script (+1 more)

### Community 121 - "scenario.py"
Cohesion: 0.19
Nodes (14): _argv(), check_variables(), texts(), visit(), _expand_steps(), load(), _number(), _parse_guard() (+6 more)

### Community 122 - "TEST"
Cohesion: 0.23
Nodes (4): ds4_report(), ds5_report(), parse(), TEST()

### Community 123 - "unistd"
Cohesion: 0.27
Nodes (5): Clock, main(), read_reports(), wait_for_disconnect(), write_output()

### Community 124 - "string"
Cohesion: 0.18
Nodes (5): BtSuppression, Ops, exists, usb_present, TEST()

### Community 125 - "closedir"
Cohesion: 0.38
Nodes (10): find_udc(), hid_id_by_mac(), open_hidraw_by_mac(), closedir(), get_tracked_dir(), is_interesting_dir(), opendir(), readdir() (+2 more)

### Community 126 - "test-binder.sh"
Cohesion: 0.24
Nodes (8): run-suites.sh script, wanted(), attach(), detach(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, test-binder.sh script

### Community 127 - "test-gadget.sh"
Cohesion: 0.35
Nodes (10): cleanup_gadget(), occupy_udc(), release_udc(), test-gadget.sh script, sony_usb_dev(), start_virtual(), stop_virtual(), usb_attr_is() (+2 more)

### Community 128 - "test-virtual.sh"
Cohesion: 0.31
Nodes (7): DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, test-virtual.sh script, start_virtual(), stop_virtual(), wait_virtual()

### Community 129 - "DriverTransportTest"
Cohesion: 0.29
Nodes (4): DriverTransportTest, dev_dir_, module_dir_, root_

### Community 130 - "hid-ds4-translator (kernel module)"
Cohesion: 0.29
Nodes (7): Build, hid-ds4-translator (kernel module), Install with DKMS, Lint and static analysis, Secure Boot, Test without hardware, Try it

### Community 131 - "rules"
Cohesion: 0.33
Nodes (5): rules, self-repository, superfluous-actions, disable, ignore

### Community 132 - "read_available"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

### Community 135 - "integration"
Cohesion: 0.67
Nodes (3): needs, runs-on, integration

### Community 139 - "broker.py"
Cohesion: 0.16
Nodes (7): serve_lines(), ServeTest, BadRequest, _execute(), _send(), serve(), _validate()

### Community 142 - "cli.py"
Cohesion: 0.16
Nodes (9): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+1 more)

### Community 145 - "steps.py"
Cohesion: 0.14
Nodes (3): StreamProcessTest, stream_process(), RoundRestart

### Community 153 - "kernel.yml"
Cohesion: 0.10
Nodes (19): container, name, runs-on, steps, jobs, build, name, on (+11 more)

### Community 154 - "vector"
Cohesion: 0.18
Nodes (18): backend_values(), build_calibrate(), build_set_backend(), build_set_mac_squat(), build_set_type(), calibration_target_values(), check_value(), contains() (+10 more)

### Community 155 - "TEST"
Cohesion: 0.16
Nodes (4): normalise(), axis(), TEST(), turn()

### Community 156 - "RootBroker"
Cohesion: 0.21
Nodes (3): RootBrokerTest, BrokerError, RootBroker

### Community 158 - "calibrationcontroller.cpp"
Cohesion: 0.28
Nodes (4): CalibrationController::CalibrationController(), CalibrationController::cancel(), CalibrationController::resetController(), CalibrationController::runNext()

### Community 159 - "fake-pad.c"
Cohesion: 0.36
Nodes (9): answer_get_report(), crc32_seeded(), create_device(), handle_events(), main(), send_input(), sleep_ms(), uhid_write() (+1 more)

### Community 160 - "Action"
Cohesion: 0.20
Nodes (10): Action, Kind, Command, Pause, Prompt, Rotate, Store, page (+2 more)

### Community 161 - "fs-util.cpp"
Cohesion: 0.53
Nodes (3): set_error(), write_all(), write_file_atomic()

### Community 164 - "TouchPoint"
Cohesion: 0.40
Nodes (5): decode_touch_point(), TouchPoint, active, x, y

### Community 165 - "BatteryState"
Cohesion: 0.50
Nodes (4): BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL

## Knowledge Gaps
- **439 isolated node(s):** `name`, `bit`, `dpad`, `name`, `arg_completions` (+434 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1147 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **47 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `daemoncontroller.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.083) - this node is a cross-community bridge._
- **Are the 35 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_calibrate()`) actually correct?**
  _`TEST()` has 35 INFERRED edges - model-reasoned connections that need verification._
- **What connects `name`, `bit`, `dpad` to the rest of the system?**
  _439 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Result` be split into smaller, more focused modules?**
  _Cohesion score 0.10285714285714286 - nodes in this community are weakly interconnected._
- **Why does `InputState` connect `InputState` to `Qt UI model (DS4 settings)`, `TouchPoint`, `string`, `ds4-ctl CLI`, `ipc-client.cpp`, `.selected`?**
  _High betweenness centrality (0.049) - this node is a cross-community bridge._
- **Are the 23 inferred relationships involving `TEST()` (e.g. with `build_ds4_output_report()` and `build_gip_input_report()`) actually correct?**
  _`TEST()` has 23 INFERRED edges - model-reasoned connections that need verification._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._