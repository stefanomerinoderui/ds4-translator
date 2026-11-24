# Graph Report - ds4-translator  (2026-11-24)

## Corpus Check
- 127 files · ~144,492 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 36 file(s) not represented in the graph (top: .qml 12, (none) 8, .toml 5)
- WARNING: 36 of 168 files seen (21%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 2231 nodes · 4313 edges · 142 communities (106 shown, 36 thin omitted)
- Extraction: 88% EXTRACTED · 12% INFERRED · 0% AMBIGUOUS · INFERRED: 512 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `c1880728`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- ControllerType
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- broker.py
- Uevent filter tests
- src/main.cpp
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
- Report building & IPC client
- scenario_from
- Bluetooth output pacer
- AppController (UI)
- FunctionFSDevice
- TrayIcon
- hid-bpf-transport.cpp
- SingleInstance
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
- string
- ds4t-virtual.c
- QString
- DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)
- gtest
- usb_hid_transport_open
- appcontroller.cpp
- close
- string
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- TEST
- TEST
- jobs
- cli.py
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
- test_usb_transport.cpp
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- translate.h
- BackendType
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
- test-virtual.sh
- simulate_arch_release.sh
- plan
- find_hid_interface
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
- virtual-check.c
- scenario.py
- ds4t_init
- hid-ds4-translator (kernel module)
- toStringList
- ReadStatus
- ioctl
- RootBroker
- test_kernel_translate.cpp
- write
- lib.sh
- SyntheticState
- paths.h
- ds4t_pad_put
- DriverTransportTest
- cstring
- Controls
- hid-driver-sysfs.h

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 58 edges
5. `TEST()` - 55 edges
6. `close()` - 46 edges
7. `AppController` - 38 edges
8. `Result` - 33 edges
9. `ioctl()` - 33 edges
10. `TransportState` - 32 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `on_ringbuf_report()` --calls--> `write()`  [INFERRED]
  src/hid-bpf-transport.cpp → tools/ds4-intercept/intercept.c
- `run_virtual_ui()` --calls--> `poll()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c
- `run_virtual_ui()` --calls--> `read()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c
- `run_test_ui()` --calls--> `close()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (142 total, 36 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.10
Nodes (11): FakeRunner, FakeUI, GuardTest, PromptTest, RepeatTest, RestoreTest, run_scenario(), RunStepTest (+3 more)

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "ControllerType"
Cohesion: 0.13
Nodes (22): backend_for_type(), create_kernel_virtual(), create_virtual_device_backend(), driver_push_ignore_mac(), ffs_target_for_type(), read_config(), read_name_config(), reconcile_driver_attachment() (+14 more)

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (47): container, needs, permissions, runs-on, steps, needs, permissions, runs-on (+39 more)

### Community 6 - "broker.py"
Cohesion: 0.08
Nodes (10): serve_lines(), ServeTest, StreamProcessTest, BadRequest, _execute(), _send(), serve(), _validate() (+2 more)

### Community 7 - "Uevent filter tests"
Cohesion: 0.12
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "src/main.cpp"
Cohesion: 0.08
Nodes (34): create_virtual_device(), destroy_virtual_device(), device_node_sysfs_path(), flush_bt_output_for_fd(), hide_method_config_str(), hide_method_from_str(), hide_method_is_unbind_family(), HideMethod (+26 more)

### Community 9 - "Executor"
Cohesion: 0.14
Nodes (9): Step, substitute(), Executor, hit(), probe(), Outcome, Runner, StepFailed (+1 more)

### Community 10 - "intercept.c"
Cohesion: 0.15
Nodes (32): find_udc(), hid_id_by_mac(), classify_path(), closedir(), decode_evdev_ioctl(), decode_hidraw_ioctl(), decode_ioctl(), dev_type_str() (+24 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.07
Nodes (21): check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script, status_field(), unit_prop(), test_daemon_ctl.sh script, client() (+13 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.06
Nodes (21): DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name, m_ds4NameIsDefault, m_dualsenseBackend (+13 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.11
Nodes (30): button_names_joined(), ButtonSpec, bit, dpad, name, CommandSpec, arg_completions, name (+22 more)

### Community 14 - "TEST_F"
Cohesion: 0.11
Nodes (14): abi_matches(), copy_field(), driver_transport_attach(), driver_transport_available(), driver_transport_create_virtual(), driver_transport_destroy_virtual(), driver_transport_inject(), driver_transport_link_virtual() (+6 more)

### Community 15 - "UI IPC client"
Cohesion: 0.09
Nodes (16): LLVMFuzzerTestOneInput(), IpcClient::closeConnection(), IpcClient::isBusy(), IpcClient::onReadable(), IpcClient::onTimeout(), IpcClient::startNext(), IpcClient, IpcClient::IpcClient() (+8 more)

### Community 16 - "libudev stub tests"
Cohesion: 0.10
Nodes (14): fake_parent(), is_tagged(), physical_ds4(), TEST(), udev_device, devpath, pid, uevent (+6 more)

### Community 17 - "ds4t-chardev.c"
Cohesion: 0.13
Nodes (13): ds4t_dbg_show(), ds4t_feature_request(), ds4t_get_calibration(), ds4t_ioctl(), ds4t_pad_find(), ds4t_pad_get(), ds4t_pad_report(), ds4t_poll() (+5 more)

### Community 18 - "HID-BPF programs"
Cohesion: 0.08
Nodes (17): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), answer_get_report(), crc32_seeded(), create_device(), handle_events() (+9 more)

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.09
Nodes (21): abort_transport(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx, device_address, ep_in (+13 more)

### Community 20 - "functionfs-backend.c"
Cohesion: 0.13
Nodes (21): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_event_packet() (+13 more)

### Community 21 - "Report building & IPC client"
Cohesion: 0.14
Nodes (39): backend_values(), build_led(), build_set_backend(), build_set_hide_method(), build_set_mac_squat(), build_set_name(), build_set_type(), check_value() (+31 more)

### Community 22 - "scenario_from"
Cohesion: 0.14
Nodes (4): scenario_from(), ParsingTest, VariablesTest, count_steps()

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.09
Nodes (16): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+8 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "FunctionFSDevice"
Cohesion: 0.08
Nodes (22): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+14 more)

### Community 27 - "TrayIcon"
Cohesion: 0.11
Nodes (12): AppController, TrayIcon::buildMenu(), DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction (+4 more)

### Community 28 - "hid-bpf-transport.cpp"
Cohesion: 0.13
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), on_ringbuf_report(), run_udev_hid_bpf() (+8 more)

### Community 29 - "SingleInstance"
Cohesion: 0.13
Nodes (8): SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance, m_server, public, SingleInstance::SingleInstance()

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.20
Nodes (7): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), DaemonController::DaemonController(), isHardwareTest()

### Community 32 - "steps"
Cohesion: 0.09
Nodes (22): runs-on, steps, jobs, analyze, name, on, pull_request, push (+14 more)

### Community 33 - "helpers.py"
Cohesion: 0.24
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

### Community 37 - "trayicon.cpp"
Cohesion: 0.12
Nodes (4): main(), appletIcon(), TrayIcon::isAvailable(), TrayIcon::TrayIcon()

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

### Community 42 - "string"
Cohesion: 0.15
Nodes (6): StatusField, key, value, ns_per_op(), report(), TEST()

### Community 43 - "ds4t-virtual.c"
Cohesion: 0.14
Nodes (13): ds4_calibration_report(), ds4t_parse_mac(), ds4t_virt_alloc(), ds4t_virt_build_pad_output(), ds4t_virt_feature(), ds4t_virt_ll_output_report(), ds4t_virt_ll_parse(), ds4t_virt_ll_raw_request() (+5 more)

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "gtest"
Cohesion: 0.13
Nodes (9): parse_hid_id(), LLVMFuzzerTestOneInput(), parse(), Parsed, bus, ok, product, vendor (+1 more)

### Community 47 - "usb_hid_transport_open"
Cohesion: 0.23
Nodes (14): device_still_present(), is_own_virtual_usb_device(), usb_hid_transport_open(), libusb_context, libusb_device, libusb_exit(), libusb_free_device_list(), libusb_get_bus_number() (+6 more)

### Community 49 - "close"
Cohesion: 0.15
Nodes (11): driver_transport_detach(), background_delayed_udevadm_trigger(), read_hid_ignore_special_drivers(), setup_ipc_socket(), FakeDaemon, dir_, handler_, listen_fd_ (+3 more)

### Community 50 - "string"
Cohesion: 0.10
Nodes (30): discard_pending_bt_output(), find_physical_ds4(), find_usb_ds4_hid_id(), get_event_nodes(), is_own_virtual_hidraw(), kernel_link_virtual(), kick_bt_full_report_mode(), load_remembered_physical_mac() (+22 more)

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
Cohesion: 0.18
Nodes (14): ds4t_build_ds4(), ds4t_build_dualsense(), ds4t_calibration_to_usb_layout(), ds4t_crc32(), ds4t_decode_ds4_battery(), ds4t_encode_dualsense_battery(), ds4t_parse_input(), le32() (+6 more)

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): jobs, performance, security-static, smoke-sanity, unit, needs, runs-on, needs (+4 more)

### Community 60 - "cli.py"
Cohesion: 0.15
Nodes (10): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+2 more)

### Community 61 - "unistd"
Cohesion: 0.14
Nodes (4): set_error(), write_all(), write_file_atomic(), Clock

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

### Community 67 - "udev-spoof.c"
Cohesion: 0.24
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

### Community 74 - "test_usb_transport.cpp"
Cohesion: 0.22
Nodes (7): Clock, connect(), fuzz(), idle(), raw(), reset(), subscribe()

### Community 75 - "hid-ds4-translator kernel module (Option A binder)"
Cohesion: 0.29
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "translate.h"
Cohesion: 0.27
Nodes (3): dualshock4_touch_report, points, timestamp

### Community 78 - "BackendType"
Cohesion: 0.25
Nodes (9): backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_KERNEL, BACKEND_UHID, read_backend_config(), write_config() (+1 more)

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
Cohesion: 0.11
Nodes (11): ds4t_control_create_virtual(), ds4t_control_device(), ds4t_control_inject(), ds4t_control_ioctl(), ds4t_control_release(), ds4t_control_set_ignore_mac(), ds4t_attach(), ds4t_detach() (+3 more)

### Community 83 - "hid-unbind-detect.cpp"
Cohesion: 0.19
Nodes (10): hid_bus_scan_existing(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_open(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+2 more)

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
Cohesion: 0.13
Nodes (14): run-suites.sh script, attach(), detach(), DS4_CONFIG_PATH, DS4_RUNTIME_DIR, DS4_STATE_DIR, test-binder.sh script, DS4_CONFIG_PATH (+6 more)

### Community 94 - "plan"
Cohesion: 0.22
Nodes (4): PlanTest, _Lenient, plan(), walk()

### Community 95 - "find_hid_interface"
Cohesion: 0.67
Nodes (3): find_hid_interface(), libusb_free_config_descriptor(), libusb_get_active_config_descriptor()

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
Cohesion: 0.19
Nodes (17): emit_input_report(), send_gip_guide_event(), BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL, build_ds4_usb_report(), build_dualsense_report() (+9 more)

### Community 105 - "integration"
Cohesion: 0.67
Nodes (3): needs, runs-on, integration

### Community 106 - "dualshock4_input_report_usb"
Cohesion: 0.33
Nodes (6): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports

### Community 107 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 113 - "virtual-check.c"
Cohesion: 0.13
Nodes (7): abs_value(), fill_injected(), main(), open_event_node(), print_state(), rumble(), sleep_ms()

### Community 114 - "scenario.py"
Cohesion: 0.19
Nodes (13): _argv(), check_variables(), texts(), visit(), _expand_steps(), load(), _number(), _parse_guard() (+5 more)

### Community 119 - "ds4t_init"
Cohesion: 0.33
Nodes (6): ds4t_debugfs_exit(), ds4t_debugfs_init(), ds4t_control_exit(), ds4t_control_init(), ds4t_exit(), ds4t_init()

### Community 122 - "hid-ds4-translator (kernel module)"
Cohesion: 0.29
Nodes (7): Build, hid-ds4-translator (kernel module), Install with DKMS, Lint and static analysis, Secure Boot, Test without hardware, Try it

### Community 123 - "toStringList"
Cohesion: 0.43
Nodes (5): DaemonController::backendValues(), DaemonController::hideMethodValues(), QStringList, DaemonController::typeValues(), toStringList()

### Community 124 - "ReadStatus"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

### Community 125 - "ioctl"
Cohesion: 0.25
Nodes (14): driver_transport_get_calibration(), do_calibration(), do_control(), do_feature_get(), do_feature_set(), do_hidraw_feature(), do_hold(), do_info() (+6 more)

### Community 126 - "RootBroker"
Cohesion: 0.21
Nodes (3): RootBrokerTest, BrokerError, RootBroker

### Community 127 - "test_kernel_translate.cpp"
Cohesion: 0.18
Nodes (9): bt_report(), daemon_parse_bt(), daemon_parse_usb(), DaemonView, common, num_touch, touch, expect_same() (+1 more)

### Community 128 - "write"
Cohesion: 0.21
Nodes (11): driver_transport_claim(), in_loop(), pump_thread_fn(), write_hid_ignore_special_drivers(), pump_thread_fn(), main(), read_reports(), wait_for_disconnect() (+3 more)

### Community 129 - "lib.sh"
Cohesion: 0.21
Nodes (6): bound_driver(), check(), hid_dev_by_mac(), nok(), ok(), lib.sh script

### Community 130 - "SyntheticState"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

### Community 131 - "paths.h"
Cohesion: 0.53
Nodes (8): config_path(), driver_dev_dir(), driver_module_dir(), from_env(), none_sentinel_path(), runtime_dir(), socket_path(), state_dir()

### Community 132 - "ds4t_pad_put"
Cohesion: 0.28
Nodes (9): ds4t_pad_put(), ds4t_pad_unregister(), ds4t_release(), ds4t_control_link_virtual(), ds4t_remove(), ds4t_virt_neutral(), ds4t_virt_unfollow(), ds4t_virtual_link() (+1 more)

### Community 134 - "DriverTransportTest"
Cohesion: 0.25
Nodes (4): DriverTransportTest, dev_dir_, module_dir_, root_

### Community 135 - "cstring"
Cohesion: 0.29
Nodes (4): bluetooth_calibration_report(), fill_payload(), le16(), neutral()

### Community 136 - "Controls"
Cohesion: 0.33
Nodes (5): Controls, feed, gone, reclaimed, wedge

## Knowledge Gaps
- **358 isolated node(s):** `name`, `bit`, `dpad`, `name`, `arg_completions` (+353 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 953 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **36 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `daemoncontroller.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.074) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `name`, `bit`, `dpad` to the rest of the system?**
  _358 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `hwtest executor tests` be split into smaller, more focused modules?**
  _Cohesion score 0.10285714285714286 - nodes in this community are weakly interconnected._
- **Why does `DaemonController` connect `DaemonController (UI)` to `daemoncontroller.h`, `QString`, `UI IPC client`, `.setPendingCommands`, `toStringList`, `daemoncontroller.cpp`?**
  _High betweenness centrality (0.050) - this node is a cross-community bridge._
- **Are the 18 inferred relationships involving `main()` (e.g. with `driver_transport_node_exists()` and `hid_bus_scan_existing()`) actually correct?**
  _`main()` has 18 INFERRED edges - model-reasoned connections that need verification._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._