# Graph Report - ds4-translator  (2026-11-26)

## Corpus Check
- 134 files · ~151,161 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 36 file(s) not represented in the graph (top: .qml 12, (none) 8, .toml 5)
- WARNING: 36 of 175 files seen (21%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 2327 nodes · 4510 edges · 145 communities (104 shown, 41 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 539 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `843446c9`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- main
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- time
- TEST_F
- fs-util.cpp
- Executor
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
- cli.py
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
- process.py
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- TEST
- TEST
- jobs
- scenario.py
- unistd
- Security findings register
- ds4t-driver.c
- Hide methods (legacy/unbind/unlink)
- TimingStats
- singleinstance.cpp
- steps
- IncludeTest
- compatibility
- ds4-translator README
- OutputState
- How DS4-Translator Works
- sockclient.py
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- read
- BtSuppression
- Bluetooth HID authorization prompt screenshot
- rules
- paths.h
- ds4t-control.c
- hid-unbind-detect.cpp
- static_checks.sh
- compatibility-arch
- dualshock4_touch_point
- bisect-capabilities.sh
- Source
- ds4-ctl-completion.bash
- security
- RootBroker
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
- translate.h
- interrupt_transfer
- TEST
- integration
- dualshock4_input_report_usb
- Debugging and Interceptor Guide
- analyze.sh
- run-vm.sh
- src/main.cpp
- broker.py
- ds4t_init
- ButtonSpec
- hid-ds4-translator (kernel module)
- string
- ioctl
- test_kernel_translate.cpp
- plan
- lib.sh
- SyntheticState
- steps.py
- SingleInstance
- Controls
- closedir
- find_hid_interface
- ReadStatus

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 62 edges
5. `TEST()` - 55 edges
6. `close()` - 47 edges
7. `AppController` - 38 edges
8. `ioctl()` - 34 edges
9. `Result` - 33 edges
10. `TransportState` - 32 edges

## Surprising Connections (you probably didn't know these)
- `in_loop()` --calls--> `write()`  [INFERRED]
  src/functionfs-backend.c → tools/ds4-intercept/intercept.c
- `LLVMFuzzerTestOneInput()` --calls--> `parse_hid_id()`  [INFERRED]
  tests/fuzz/fuzz_parsers.cpp → src/hid-unbind-detect.cpp
- `on_ringbuf_report()` --calls--> `write()`  [INFERRED]
  src/hid-bpf-transport.cpp → tools/ds4-intercept/intercept.c
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `write_file_str()` --calls--> `close()`  [INFERRED]
  src/functionfs-backend.c → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (145 total, 41 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.10
Nodes (11): FakeRunner, FakeUI, GuardTest, PromptTest, RepeatTest, RestoreTest, run_scenario(), RunStepTest (+3 more)

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "main"
Cohesion: 0.07
Nodes (50): driver_transport_destroy_virtual(), functionfs_set_mac(), backend_for_type(), backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_KERNEL (+42 more)

### Community 4 - "DS4 report decoding & mapping"
Cohesion: 0.06
Nodes (20): driver_transport_inject(), functionfs_send_input_report(), emit_input_report(), build_ds4_usb_report(), build_dualsense_report(), build_gip_input_report(), build_xbox360_report(), calculate_crc32() (+12 more)

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (47): container, needs, permissions, runs-on, steps, needs, permissions, runs-on (+39 more)

### Community 6 - "time"
Cohesion: 0.24
Nodes (6): buzz_evdev(), buzz_hidraw(), main(), read_text(), steam_pad(), sysfs_nodes()

### Community 7 - "TEST_F"
Cohesion: 0.12
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "fs-util.cpp"
Cohesion: 0.70
Nodes (3): set_error(), write_all(), write_file_atomic()

### Community 9 - "Executor"
Cohesion: 0.24
Nodes (5): Step, Executor, hit(), probe(), Outcome

### Community 10 - "intercept.c"
Cohesion: 0.21
Nodes (22): classify_path(), decode_evdev_ioctl(), decode_hidraw_ioctl(), decode_ioctl(), dev_type_str(), dup(), dup2(), dup3() (+14 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.07
Nodes (31): expect(), expect_includes(), run_cases(), test_completion.sh script, check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script (+23 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.05
Nodes (21): DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name, m_ds4NameIsDefault, m_dualsenseBackend (+13 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.13
Nodes (29): button_names_joined(), CommandSpec, arg_completions, name, find_command(), is_in_list(), main(), mark() (+21 more)

### Community 14 - "TEST_F"
Cohesion: 0.07
Nodes (25): abi_matches(), copy_field(), driver_transport_attach(), driver_transport_available(), driver_transport_claim(), driver_transport_create_virtual(), driver_transport_detach(), driver_transport_get_calibration() (+17 more)

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
Nodes (18): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_event_packet() (+10 more)

### Community 21 - "ipc-client.cpp"
Cohesion: 0.14
Nodes (39): backend_values(), build_led(), build_set_backend(), build_set_hide_method(), build_set_mac_squat(), build_set_name(), build_set_type(), check_value() (+31 more)

### Community 22 - "scenario_from"
Cohesion: 0.16
Nodes (3): scenario_from(), ParsingTest, VariablesTest

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.08
Nodes (21): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+13 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "FunctionFSDevice"
Cohesion: 0.05
Nodes (38): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+30 more)

### Community 27 - "TrayIcon"
Cohesion: 0.13
Nodes (11): AppController, DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction, m_tray (+3 more)

### Community 28 - "hid-bpf-transport.cpp"
Cohesion: 0.12
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), on_ringbuf_report(), run_udev_hid_bpf() (+8 more)

### Community 29 - "cli.py"
Cohesion: 0.16
Nodes (9): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+1 more)

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.14
Nodes (13): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), DaemonController::backendValues(), DaemonController::DaemonController(), DaemonController::hideMethodValues() (+5 more)

### Community 32 - "steps"
Cohesion: 0.09
Nodes (22): runs-on, steps, jobs, analyze, name, on, pull_request, push (+14 more)

### Community 33 - "helpers.py"
Cohesion: 0.21
Nodes (3): CliTest, run_cli(), scenario_file()

### Community 34 - "TEST_F"
Cohesion: 0.14
Nodes (3): usb_hid_transport_start(), TEST_F(), UsbTransportTest

### Community 35 - "lint.yml"
Cohesion: 0.12
Nodes (22): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+14 more)

### Community 36 - "InputState"
Cohesion: 0.08
Nodes (24): decode_buttons(), decode_touch_point(), format_state_line(), InputState, accel, buttons, dpad, gyro (+16 more)

### Community 38 - "TEST_F"
Cohesion: 0.15
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "libusb_stub.cpp"
Cohesion: 0.23
Nodes (14): usb_hid_transport_close(), Clock, libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_detach_kernel_driver(), libusb_device_handle, pending (+6 more)

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
Nodes (15): ds4_calibration_report(), ds4t_parse_mac(), ds4t_virt_alloc(), ds4t_virt_build_pad_output(), ds4t_virt_feature(), ds4t_virt_forward_feature(), ds4t_virt_get_feature(), ds4t_virt_ll_output_report() (+7 more)

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "gtest"
Cohesion: 0.14
Nodes (7): parse(), Parsed, bus, ok, product, vendor, TEST()

### Community 47 - "usb_hid_transport_open"
Cohesion: 0.21
Nodes (15): device_still_present(), is_own_virtual_usb_device(), usb_hid_transport_open(), libusb_context, libusb_device, libusb_error_name(), libusb_exit(), libusb_free_device_list() (+7 more)

### Community 49 - "close"
Cohesion: 0.13
Nodes (13): bind, hid_uevent_monitor_open(), background_delayed_udevadm_trigger(), read_hid_ignore_special_drivers(), setup_ipc_socket(), write_hid_ignore_special_drivers(), FakeDaemon, dir_ (+5 more)

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
Cohesion: 0.29
Nodes (13): ds4t_build_ds4(), ds4t_build_dualsense(), ds4t_calibration_to_usb_layout(), ds4t_crc32(), ds4t_decode_ds4_battery(), ds4t_encode_dualsense_battery(), ds4t_parse_input(), ds4t_parse_output() (+5 more)

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): jobs, performance, security-static, smoke-sanity, unit, needs, runs-on, needs (+4 more)

### Community 60 - "scenario.py"
Cohesion: 0.20
Nodes (12): _argv(), check_variables(), texts(), visit(), _expand_steps(), load(), _number(), _parse_guard() (+4 more)

### Community 62 - "Security findings register"
Cohesion: 0.20
Nodes (10): H2 control socket access policy (SO_PEERCRED, ds4-translator group), L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, ds4-ctl runtime control utility, ds4-translator daemon, Test suite (make test targets), ds4-translator-ui Qt target (+2 more)

### Community 63 - "ds4t-driver.c"
Cohesion: 0.13
Nodes (14): ds4t_pad_alloc(), ds4t_pad_register(), ds4t_mac_ignored(), ds4t_attached(), ds4t_bus_event(), ds4t_is_emulated(), ds4t_probe(), ds4t_raw_event() (+6 more)

### Community 64 - "Hide methods (legacy/unbind/unlink)"
Cohesion: 0.27
Nodes (10): hid-generic swap, Hide methods (legacy/unbind/unlink), MAC squat, Unlink of /dev nodes and reapply sweep, Workaround stack the module would replace, Physical controller hiding, hid-bpf-probe feasibility spike, HID-BPF hid_device_event swallow and ringbuf mirror (+2 more)

### Community 66 - "TimingStats"
Cohesion: 0.20
Nodes (9): TimingStats, interval_max_us, interval_mean_us, interval_min_us, latency_max_us, latency_mean_us, reports, window_us (+1 more)

### Community 67 - "singleinstance.cpp"
Cohesion: 0.22
Nodes (3): SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance::SingleInstance()

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

### Community 74 - "sockclient.py"
Cohesion: 0.33
Nodes (6): connect(), fuzz(), idle(), raw(), reset(), subscribe()

### Community 75 - "hid-ds4-translator kernel module (Option A binder)"
Cohesion: 0.29
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "read"
Cohesion: 0.33
Nodes (9): pump_thread_fn(), pump_thread_fn(), do_hold(), do_stress(), evdev_type_name(), hex_dump(), poll(), read() (+1 more)

### Community 78 - "BtSuppression"
Cohesion: 0.10
Nodes (10): BtSuppression, Ops, exists, usb_present, FakeBus, bind_attempts, bt_bound, bt_exists (+2 more)

### Community 79 - "Bluetooth HID authorization prompt screenshot"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 80 - "rules"
Cohesion: 0.33
Nodes (5): rules, self-repository, superfluous-actions, disable, ignore

### Community 81 - "paths.h"
Cohesion: 0.53
Nodes (8): config_path(), driver_dev_dir(), driver_module_dir(), from_env(), none_sentinel_path(), runtime_dir(), socket_path(), state_dir()

### Community 82 - "ds4t-control.c"
Cohesion: 0.11
Nodes (11): ds4t_control_create_virtual(), ds4t_control_device(), ds4t_control_inject(), ds4t_control_ioctl(), ds4t_control_release(), ds4t_control_set_ignore_mac(), ds4t_attach(), ds4t_detach() (+3 more)

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

### Community 91 - "RootBroker"
Cohesion: 0.21
Nodes (3): RootBrokerTest, BrokerError, RootBroker

### Community 92 - "test-virtual.sh"
Cohesion: 0.08
Nodes (25): run-suites.sh script, attach(), detach(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, test-binder.sh script, cleanup_hotswap() (+17 more)

### Community 94 - "ds4t_pad_put"
Cohesion: 0.28
Nodes (9): ds4t_pad_put(), ds4t_pad_unregister(), ds4t_release(), ds4t_control_link_virtual(), ds4t_remove(), ds4t_virt_neutral(), ds4t_virt_unfollow(), ds4t_virtual_link() (+1 more)

### Community 95 - "vector"
Cohesion: 0.21
Nodes (5): reapply_unlinked_nodes(), ds4_report(), ds5_report(), parse(), TEST()

### Community 101 - "steps"
Cohesion: 0.10
Nodes (19): container, runs-on, steps, jobs, build, name, on, pull_request (+11 more)

### Community 102 - "translate.h"
Cohesion: 0.09
Nodes (21): BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL, dualsense_touch_point, contact, x_hi, x_lo (+13 more)

### Community 103 - "interrupt_transfer"
Cohesion: 0.31
Nodes (7): cancel_active_transfer(), interrupt_transfer(), transfer_status_to_error(), libusb_alloc_transfer(), libusb_cancel_transfer(), libusb_free_transfer(), libusb_submit_transfer()

### Community 105 - "integration"
Cohesion: 0.67
Nodes (3): needs, runs-on, integration

### Community 106 - "dualshock4_input_report_usb"
Cohesion: 0.33
Nodes (6): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports

### Community 107 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 112 - "src/main.cpp"
Cohesion: 0.06
Nodes (40): functionfs_set_calibration(), device_node_sysfs_path(), discard_pending_bt_output(), driver_push_ignore_mac(), find_usb_ds4_hid_id(), get_event_nodes(), kernel_link_virtual(), kick_bt_full_report_mode() (+32 more)

### Community 113 - "broker.py"
Cohesion: 0.17
Nodes (7): serve_lines(), ServeTest, BadRequest, _execute(), _send(), serve(), _validate()

### Community 114 - "ds4t_init"
Cohesion: 0.33
Nodes (6): ds4t_debugfs_exit(), ds4t_debugfs_init(), ds4t_control_exit(), ds4t_control_init(), ds4t_exit(), ds4t_init()

### Community 119 - "ButtonSpec"
Cohesion: 0.50
Nodes (4): ButtonSpec, bit, dpad, name

### Community 122 - "hid-ds4-translator (kernel module)"
Cohesion: 0.29
Nodes (7): Build, hid-ds4-translator (kernel module), Install with DKMS, Lint and static analysis, Secure Boot, Test without hardware, Try it

### Community 124 - "string"
Cohesion: 0.21
Nodes (6): StatusField, key, value, LLVMFuzzerTestOneInput(), write_hid_driver_sysfs(), write_hid_driver_sysfs_for()

### Community 125 - "ioctl"
Cohesion: 0.13
Nodes (21): do_calibration(), do_control(), do_feature_get(), do_feature_set(), do_hidraw_feature(), do_info(), do_read(), do_write() (+13 more)

### Community 127 - "test_kernel_translate.cpp"
Cohesion: 0.15
Nodes (10): bt_report(), daemon_parse_bt(), daemon_parse_usb(), DaemonView, common, num_touch, touch, expect_same() (+2 more)

### Community 128 - "plan"
Cohesion: 0.18
Nodes (6): PlanTest, substitute(), replace(), _Lenient, plan(), walk()

### Community 129 - "lib.sh"
Cohesion: 0.21
Nodes (6): bound_driver(), check(), hid_dev_by_mac(), nok(), ok(), lib.sh script

### Community 130 - "SyntheticState"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

### Community 132 - "steps.py"
Cohesion: 0.13
Nodes (6): count_steps(), Scenario, RoundRestart, Runner, StepFailed, UI

### Community 133 - "SingleInstance"
Cohesion: 0.19
Nodes (7): main(), SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance, m_server, public, TrayIcon::isAvailable()

### Community 136 - "Controls"
Cohesion: 0.33
Nodes (5): Controls, feed, gone, reclaimed, wedge

### Community 137 - "closedir"
Cohesion: 0.38
Nodes (10): find_udc(), hid_id_by_mac(), open_hidraw_by_mac(), closedir(), get_tracked_dir(), is_interesting_dir(), opendir(), readdir() (+2 more)

### Community 142 - "find_hid_interface"
Cohesion: 0.67
Nodes (3): find_hid_interface(), libusb_free_config_descriptor(), libusb_get_active_config_descriptor()

### Community 147 - "ReadStatus"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

## Knowledge Gaps
- **368 isolated node(s):** `DS4_RUNTIME_DIR`, `DS4_STATE_DIR`, `DS4_CONFIG_PATH`, `DS4_TEST_UHID_USB_IS_PHYSICAL`, `kSourceCount` (+363 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 990 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **41 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `inputmonitor.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.103) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `DS4_RUNTIME_DIR`, `DS4_STATE_DIR`, `DS4_CONFIG_PATH` to the rest of the system?**
  _368 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `hwtest executor tests` be split into smaller, more focused modules?**
  _Cohesion score 0.10285714285714286 - nodes in this community are weakly interconnected._
- **Why does `close()` connect `close` to `main`, `InputMonitor::disconnectStream`, `libusb_stub.cpp`, `fs-util.cpp`, `TEST_F`, `intercept.c`, `inputmonitor.cpp`, `ds4-ctl CLI`, `TEST_F`, `UI IPC client`, `src/main.cpp`, `functionfs-backend.c`, `ipc-client.cpp`, `FunctionFSDevice`, `TEST`, `hid-bpf-transport.cpp`, `ioctl`?**
  _High betweenness centrality (0.051) - this node is a cross-community bridge._
- **Are the 22 inferred relationships involving `main()` (e.g. with `.pending()` and `.poll()`) actually correct?**
  _`main()` has 22 INFERRED edges - model-reasoned connections that need verification._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._