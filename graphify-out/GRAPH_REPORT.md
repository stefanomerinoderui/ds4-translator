# Graph Report - ds4-translator  (2026-11-24)

## Corpus Check
- 115 files · ~134,496 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 36 file(s) not represented in the graph (top: .qml 12, (none) 8, .toml 5)
- WARNING: 36 of 156 files seen (23%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 2103 nodes · 4023 edges · 126 communities (97 shown, 29 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 454 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `11deb122`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- src/main.cpp
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- broker.py
- Uevent filter tests
- string
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
- ControllerType
- Report building & IPC client
- scenario_from
- Bluetooth output pacer
- AppController (UI)
- FunctionFSDevice
- TrayIcon
- hid-bpf-transport.cpp
- singleinstance.cpp
- Console
- daemoncontroller.cpp
- steps
- helpers.py
- TEST_F
- lint.yml
- InputState
- trayicon.cpp
- TEST_F
- libusb_device_handle
- inputmonitor.cpp
- dualsense_input_report
- string
- fake-pad.c
- TrayIcon::summary
- DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)
- gtest
- usb_hid_transport_open
- appcontroller.cpp
- close
- ioctl
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- TEST
- TEST
- jobs
- FakeUI
- test_driver_transport.cpp
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
- atomic
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- SingleInstance
- DriverTransportTest
- Bluetooth HID authorization prompt screenshot
- read
- with
- ds4t-control.c
- hid-unbind-detect.cpp
- static_checks.sh
- compatibility-arch
- Arch pacman repository setup
- bisect-capabilities.sh
- Source
- ds4-ctl-completion.bash
- security
- unit
- test-binder.sh
- simulate_arch_release.sh
- test_steps.py
- rebind-unbound-controllers.sh
- bt-disconnect.sh
- build.sh
- InputMonitor::disconnectStream
- Graphify usage rules
- steps
- translate.h
- interrupt_transfer
- ControllerType
- dualshock4_input_report_usb
- Debugging and Interceptor Guide
- run-vm.sh
- ds4t_pad_report
- ds4t_init
- hid-ds4-translator (kernel module)
- toStringList
- ReadStatus
- ds4t_poll
- SyntheticState
- functionfs-backend.c

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 57 edges
5. `TEST()` - 55 edges
6. `close()` - 45 edges
7. `AppController` - 38 edges
8. `Result` - 33 edges
9. `TransportState` - 32 edges
10. `IpcClient` - 31 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `LLVMFuzzerTestOneInput()` --calls--> `parse_hid_id()`  [INFERRED]
  tests/fuzz/fuzz_parsers.cpp → src/hid-unbind-detect.cpp
- `abi_matches()` --calls--> `ioctl()`  [INFERRED]
  src/driver-transport.cpp → tools/ds4-intercept/intercept.c
- `driver_transport_attach()` --calls--> `close()`  [INFERRED]
  src/driver-transport.cpp → tools/ds4-intercept/intercept.c
- `driver_transport_attach()` --calls--> `ioctl()`  [INFERRED]
  src/driver-transport.cpp → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (126 total, 29 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.19
Nodes (7): FakeRunner, RepeatTest, RestoreTest, run_scenario(), RunStepTest, WaitForTest, Result

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "src/main.cpp"
Cohesion: 0.08
Nodes (22): functionfs_send_event_packet(), functionfs_set_calibration(), create_virtual_device(), create_virtual_device_backend(), destroy_virtual_device(), device_node_sysfs_path(), discard_pending_bt_output(), ffs_target_for_type() (+14 more)

### Community 3 - "Translation unit tests"
Cohesion: 0.04
Nodes (3): decode_buttons(), parse_state(), TEST()

### Community 4 - "DS4 report decoding & mapping"
Cohesion: 0.06
Nodes (7): calculate_crc32(), ds4_calibration_to_usb_layout(), bluetooth_calibration_report(), fill_payload(), le16(), neutral(), TEST()

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (45): container, needs, permissions, runs-on, steps, needs, runs-on, steps (+37 more)

### Community 6 - "broker.py"
Cohesion: 0.06
Nodes (18): RootBrokerTest, serve_lines(), ServeTest, StreamProcessTest, connect(), fuzz(), idle(), raw() (+10 more)

### Community 7 - "Uevent filter tests"
Cohesion: 0.12
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "string"
Cohesion: 0.11
Nodes (28): find_physical_ds4(), find_usb_ds4_hid_id(), get_event_nodes(), is_own_virtual_hidraw(), kick_bt_full_report_mode(), load_remembered_physical_mac(), open_and_hide_physical(), open_and_hide_physical_unbind() (+20 more)

### Community 9 - "cli.py"
Cohesion: 0.05
Nodes (37): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+29 more)

### Community 10 - "intercept.c"
Cohesion: 0.14
Nodes (35): find_udc(), background_delayed_udevadm_trigger(), read_hid_ignore_special_drivers(), setup_ipc_socket(), classify_path(), close(), closedir(), decode_evdev_ioctl() (+27 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.09
Nodes (23): check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script, status_field(), unit_prop(), test_daemon_ctl.sh script, client() (+15 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.05
Nodes (23): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name (+15 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.09
Nodes (38): button_names_joined(), ButtonSpec, bit, dpad, name, CommandSpec, arg_completions, name (+30 more)

### Community 14 - "TEST_F"
Cohesion: 0.12
Nodes (14): abi_matches(), driver_transport_attach(), driver_transport_available(), driver_transport_claim(), driver_transport_detach(), driver_transport_node_exists(), driver_transport_open(), driver_transport_set_ignore_mac() (+6 more)

### Community 15 - "UI IPC client"
Cohesion: 0.09
Nodes (15): IpcClient::closeConnection(), IpcClient::isBusy(), IpcClient::onReadable(), IpcClient::onTimeout(), IpcClient::startNext(), IpcClient, IpcClient::IpcClient(), m_buffer (+7 more)

### Community 16 - "libudev stub tests"
Cohesion: 0.10
Nodes (14): fake_parent(), is_tagged(), physical_ds4(), TEST(), udev_device, devpath, pid, uevent (+6 more)

### Community 17 - "ds4t-chardev.c"
Cohesion: 0.16
Nodes (12): ds4t_dbg_show(), ds4t_feature_request(), ds4t_get_calibration(), ds4t_ioctl(), ds4t_pad_put(), ds4t_pad_unregister(), ds4t_read(), ds4t_release() (+4 more)

### Community 18 - "HID-BPF programs"
Cohesion: 0.11
Nodes (8): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG()

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.08
Nodes (22): abort_transport(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx, device_address, ep_in (+14 more)

### Community 20 - "ControllerType"
Cohesion: 0.11
Nodes (25): backend_for_type(), backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_UHID, hide_method_config_str(), hide_method_from_str() (+17 more)

### Community 21 - "Report building & IPC client"
Cohesion: 0.16
Nodes (31): backend_values(), build_led(), build_set_backend(), build_set_hide_method(), build_set_mac_squat(), build_set_name(), build_set_type(), check_value() (+23 more)

### Community 22 - "scenario_from"
Cohesion: 0.17
Nodes (3): scenario_from(), ParsingTest, VariablesTest

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.10
Nodes (17): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+9 more)

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
Cohesion: 0.09
Nodes (17): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), pump_thread_fn(), run_udev_hid_bpf() (+9 more)

### Community 29 - "singleinstance.cpp"
Cohesion: 0.22
Nodes (3): SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance::SingleInstance()

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.22
Nodes (5): DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), DaemonController::DaemonController(), isHardwareTest()

### Community 32 - "steps"
Cohesion: 0.09
Nodes (21): runs-on, steps, jobs, analyze, name, on, pull_request, push (+13 more)

### Community 33 - "helpers.py"
Cohesion: 0.22
Nodes (3): CliTest, run_cli(), scenario_file()

### Community 34 - "TEST_F"
Cohesion: 0.14
Nodes (3): usb_hid_transport_start(), TEST_F(), UsbTransportTest

### Community 35 - "lint.yml"
Cohesion: 0.12
Nodes (20): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+12 more)

### Community 36 - "InputState"
Cohesion: 0.09
Nodes (21): decode_touch_point(), InputState, accel, buttons, dpad, gyro, has_motion, has_touch (+13 more)

### Community 37 - "trayicon.cpp"
Cohesion: 0.10
Nodes (5): main(), appletIcon(), TrayIcon::buildMenu(), TrayIcon::isAvailable(), TrayIcon::TrayIcon()

### Community 38 - "TEST_F"
Cohesion: 0.15
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "libusb_device_handle"
Cohesion: 0.29
Nodes (11): cancel_active_transfer(), usb_hid_transport_close(), libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_detach_kernel_driver(), libusb_device_handle, pending (+3 more)

### Community 40 - "inputmonitor.cpp"
Cohesion: 0.18
Nodes (7): InputMonitor::clearStates(), InputMonitor::dpadName(), InputMonitor::measure(), InputMonitor::note(), InputMonitor::onReadable(), InputMonitor::tryConnect(), InputMonitor::InputMonitor()

### Community 41 - "dualsense_input_report"
Cohesion: 0.11
Nodes (18): dualsense_input_report, accel, buttons, gyro, points, reserved, reserved2, reserved3 (+10 more)

### Community 42 - "string"
Cohesion: 0.16
Nodes (7): StatusField, key, value, LLVMFuzzerTestOneInput(), LLVMFuzzerTestOneInput(), write_hid_driver_sysfs(), write_hid_driver_sysfs_for()

### Community 43 - "fake-pad.c"
Cohesion: 0.18
Nodes (9): answer_get_report(), crc32_seeded(), create_device(), handle_events(), main(), send_input(), sleep_ms(), uhid_write() (+1 more)

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "gtest"
Cohesion: 0.15
Nodes (7): parse(), Parsed, bus, ok, product, vendor, TEST()

### Community 47 - "usb_hid_transport_open"
Cohesion: 0.15
Nodes (21): device_still_present(), find_hid_interface(), is_own_virtual_usb_device(), usb_hid_transport_open(), Clock, libusb_context, libusb_device, libusb_error_name() (+13 more)

### Community 49 - "close"
Cohesion: 0.16
Nodes (6): FakeDaemon, dir_, handler_, listen_fd_, thread_, read_command()

### Community 50 - "ioctl"
Cohesion: 0.21
Nodes (13): driver_transport_get_calibration(), do_calibration(), do_control(), do_feature_get(), do_feature_set(), do_hidraw_feature(), do_info(), do_read() (+5 more)

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
Cohesion: 0.36
Nodes (13): steps, steps, steps, steps, steps, steps, steps, continue-on-error (+5 more)

### Community 56 - "DualShock 4 base layout diagram (unlabelled SVG)"
Cohesion: 0.17
Nodes (12): Shoulder bumpers L1/R1, DualShock 4 controller, DualShock 4 base layout diagram (unlabelled SVG), D-pad, Face buttons (cross, circle, square, triangle), Light bar, PS button, Share and Options buttons (+4 more)

### Community 57 - "TEST"
Cohesion: 0.28
Nodes (3): ns_per_op(), report(), TEST()

### Community 58 - "TEST"
Cohesion: 0.12
Nodes (10): connect_socket(), errno_text(), read_available(), send_command(), set_non_blocking(), set_timeout(), trim_response(), write_all() (+2 more)

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): needs, runs-on, jobs, integration, performance, security-static, smoke-sanity, needs (+4 more)

### Community 61 - "test_driver_transport.cpp"
Cohesion: 0.17
Nodes (5): set_error(), write_all(), write_file_atomic(), Clock, Clock

### Community 62 - "Security findings register"
Cohesion: 0.20
Nodes (10): H2 control socket access policy (SO_PEERCRED, ds4-translator group), L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, ds4-ctl runtime control utility, ds4-translator daemon, Test suite (make test targets), ds4-translator-ui Qt target (+2 more)

### Community 63 - "ds4t-driver.c"
Cohesion: 0.13
Nodes (14): ds4t_pad_alloc(), ds4t_pad_register(), ds4t_control_release(), ds4t_mac_ignored(), ds4t_attached(), ds4t_bus_event(), ds4t_detach(), ds4t_is_emulated() (+6 more)

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
Cohesion: 0.32
Nodes (8): Backend selection (uhid vs functionfs), Known issues (Steam MAC, BT auth popup), ds4-translator README, USB priority live hot-swap, Xbox 360/One/Series emulation, hwtest guided hardware test runner, Single pkexec root helper, hwtest TOML scenario format

### Community 72 - "OutputState"
Cohesion: 0.25
Nodes (8): format_output_line(), OutputState, blue, green, red, rumble_left, rumble_right, parse_output()

### Community 73 - "How DS4-Translator Works"
Cohesion: 0.33
Nodes (7): FunctionFS backend, How DS4-Translator Works, Haptics rumble heuristic, Emulation type none, udev hide rules (72-ds4-translator-hide.rules), Virtual controller identity and feature reports, Light bar flash passthrough

### Community 74 - "atomic"
Cohesion: 0.33
Nodes (5): Controls, feed, gone, reclaimed, wedge

### Community 75 - "hid-ds4-translator kernel module (Option A binder)"
Cohesion: 0.29
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "SingleInstance"
Cohesion: 0.24
Nodes (5): SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance, m_server, public

### Community 78 - "DriverTransportTest"
Cohesion: 0.25
Nodes (4): DriverTransportTest, dev_dir_, module_dir_, root_

### Community 79 - "Bluetooth HID authorization prompt screenshot"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 80 - "read"
Cohesion: 0.36
Nodes (7): pump_thread_fn(), do_hold(), read_reports(), wait_for_disconnect(), evdev_type_name(), poll(), read()

### Community 81 - "with"
Cohesion: 0.40
Nodes (5): with, fetch-depth, if-no-files-found, name, path

### Community 82 - "ds4t-control.c"
Cohesion: 0.18
Nodes (3): ds4t_control_ioctl(), ds4t_control_set_ignore_mac(), ds4t_attach()

### Community 83 - "hid-unbind-detect.cpp"
Cohesion: 0.21
Nodes (11): hid_bus_scan_existing(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_open(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+3 more)

### Community 85 - "compatibility-arch"
Cohesion: 0.50
Nodes (4): container, needs, runs-on, compatibility-arch

### Community 86 - "Arch pacman repository setup"
Cohesion: 0.50
Nodes (4): pacman repository landing page, arch-repo release CI job and GitHub Pages deploy, Arch pacman repository setup, Package signing with release subkey

### Community 88 - "Source"
Cohesion: 0.50
Nodes (3): Source, Physical, Virtual

### Community 90 - "security"
Cohesion: 0.67
Nodes (3): security, needs, runs-on

### Community 91 - "unit"
Cohesion: 0.67
Nodes (3): unit, needs, runs-on

### Community 92 - "test-binder.sh"
Cohesion: 0.18
Nodes (14): attach(), bound_driver(), check(), detach(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, hid_dev_by_mac() (+6 more)

### Community 94 - "test_steps.py"
Cohesion: 0.31
Nodes (3): GuardTest, PlanTest, status()

### Community 101 - "steps"
Cohesion: 0.11
Nodes (18): container, runs-on, steps, jobs, build, name, on, pull_request (+10 more)

### Community 102 - "translate.h"
Cohesion: 0.08
Nodes (23): dualsense_touch_point, contact, x_hi, x_lo, y_hi, y_lo, dualshock4_input_report_bt, common (+15 more)

### Community 103 - "interrupt_transfer"
Cohesion: 0.36
Nodes (6): interrupt_transfer(), transfer_status_to_error(), libusb_alloc_transfer(), libusb_cancel_transfer(), libusb_free_transfer(), libusb_submit_transfer()

### Community 104 - "ControllerType"
Cohesion: 0.11
Nodes (30): emit_input_report(), read_config(), read_name_config(), BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL, build_gip_input_report() (+22 more)

### Community 106 - "dualshock4_input_report_usb"
Cohesion: 0.33
Nodes (6): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports

### Community 107 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 114 - "ds4t_pad_report"
Cohesion: 0.50
Nodes (3): ds4t_pad_report(), ds4t_ring_push(), ds4t_raw_event()

### Community 119 - "ds4t_init"
Cohesion: 0.33
Nodes (6): ds4t_debugfs_exit(), ds4t_debugfs_init(), ds4t_control_exit(), ds4t_control_init(), ds4t_exit(), ds4t_init()

### Community 122 - "hid-ds4-translator (kernel module)"
Cohesion: 0.33
Nodes (6): Build, hid-ds4-translator (kernel module), Install with DKMS, Secure Boot, Test without hardware, Try it

### Community 123 - "toStringList"
Cohesion: 0.43
Nodes (5): DaemonController::backendValues(), DaemonController::hideMethodValues(), QStringList, DaemonController::typeValues(), toStringList()

### Community 124 - "ReadStatus"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

### Community 130 - "SyntheticState"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

### Community 135 - "functionfs-backend.c"
Cohesion: 0.13
Nodes (21): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_input_report() (+13 more)

## Knowledge Gaps
- **351 isolated node(s):** `fd`, `orig_mode`, `is_grabbed`, `is_unlinked`, `rdev` (+346 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 913 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **29 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `daemoncontroller.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.099) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `fd`, `orig_mode`, `is_grabbed` to the rest of the system?**
  _351 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._
- **Why does `close()` connect `intercept.c` to `src/main.cpp`, `functionfs-backend.c`, `string`, `Uevent filter tests`, `ds4-ctl CLI`, `TEST_F`, `UI IPC client`, `ControllerType`, `hid-bpf-transport.cpp`, `libusb_device_handle`, `inputmonitor.cpp`, `string`, `fake-pad.c`, `close`, `ioctl`, `TEST`, `test_driver_transport.cpp`, `hid-unbind-detect.cpp`, `InputMonitor::disconnectStream`?**
  _High betweenness centrality (0.064) - this node is a cross-community bridge._
- **Are the 18 inferred relationships involving `main()` (e.g. with `driver_transport_node_exists()` and `hid_bus_scan_existing()`) actually correct?**
  _`main()` has 18 INFERRED edges - model-reasoned connections that need verification._
- **Should `src/main.cpp` be split into smaller, more focused modules?**
  _Cohesion score 0.07936507936507936 - nodes in this community are weakly interconnected._