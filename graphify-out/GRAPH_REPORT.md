# Graph Report - ds4-translator  (2026-11-23)

## Corpus Check
- 105 files · ~123,482 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 33 file(s) not represented in the graph (top: .qml 12, (none) 7, .toml 5)
- WARNING: 33 of 143 files seen (23%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 1935 nodes · 3713 edges · 112 communities (88 shown, 24 thin omitted)
- Extraction: 90% EXTRACTED · 10% INFERRED · 0% AMBIGUOUS · INFERRED: 386 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `cdafba94`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- Daemon system headers & udev/hidraw
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- hwtest runtime (process/broker)
- Uevent filter tests
- main
- cli.py
- intercept.c
- Hardware verify.sh
- DaemonController (UI)
- ds4-ctl CLI
- IPC client tests
- UI IPC client
- libudev stub tests
- hid-ds4-translator.c
- HID-BPF programs
- USB HID transport (libusb)
- translate.h
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
- RootBroker
- QString
- DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)
- gtest
- libusb_device
- appcontroller.cpp
- check_value
- Button
- dualshock4_input_report_common
- Button
- tests.yml
- name
- DualShock 4 base layout diagram (unlabelled SVG)
- TEST
- udev-spoof.c
- jobs
- time
- PhysicalNode
- Security findings register
- SyntheticState
- Hide methods (legacy/unbind/unlink)
- TimingStats
- process.py
- steps
- IncludeTest
- compatibility
- ds4-translator README
- OutputState
- How DS4-Translator Works
- atomic
- hid-ds4-translator kernel module (Option A binder)
- Unimplemented DualShock 4 features
- dualshock4_touch_point
- QStringList
- Bluetooth HID authorization prompt screenshot
- cstring
- with
- hid-driver-sysfs.h
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
- fake-pad.c
- DaemonController::DaemonController
- rebind-unbound-controllers.sh
- bt-disconnect.sh
- build.sh
- InputMonitor::disconnectStream
- Graphify usage rules
- singleinstance.cpp
- dualshock4_input_report_bt
- interrupt_transfer
- TrayIcon::TrayIcon
- dualsense_touch_point
- dualshock4_input_report_usb
- Debugging and Interceptor Guide
- StatusField
- TEST
- run-vm.sh

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 54 edges
5. `TEST()` - 47 edges
6. `close()` - 39 edges
7. `AppController` - 38 edges
8. `Result` - 33 edges
9. `TransportState` - 32 edges
10. `IpcClient` - 31 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `on_ringbuf_report()` --calls--> `write()`  [INFERRED]
  src/hid-bpf-transport.cpp → tools/ds4-intercept/intercept.c
- `LLVMFuzzerTestOneInput()` --calls--> `parse_hid_id()`  [INFERRED]
  tests/fuzz/fuzz_parsers.cpp → src/hid-unbind-detect.cpp
- `uhid_write()` --calls--> `write()`  [INFERRED]
  tests/kernel/fake-pad.c → tools/ds4-intercept/intercept.c
- `handle_events()` --calls--> `read()`  [INFERRED]
  tests/kernel/fake-pad.c → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]

## Communities (112 total, 24 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.10
Nodes (11): FakeRunner, FakeUI, GuardTest, PromptTest, RepeatTest, RestoreTest, run_scenario(), RunStepTest (+3 more)

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "Daemon system headers & udev/hidraw"
Cohesion: 0.11
Nodes (32): create_virtual_device(), create_virtual_device_backend(), destroy_virtual_device(), device_node_sysfs_path(), ffs_target_for_type(), find_physical_ds4(), find_usb_ds4_hid_id(), hidraw_to_hid_id() (+24 more)

### Community 4 - "DS4 report decoding & mapping"
Cohesion: 0.05
Nodes (33): backend_for_type(), emit_input_report(), read_config(), send_gip_guide_event(), BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL (+25 more)

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (45): container, needs, permissions, runs-on, steps, needs, runs-on, steps (+37 more)

### Community 6 - "hwtest runtime (process/broker)"
Cohesion: 0.14
Nodes (7): serve_lines(), ServeTest, BadRequest, _execute(), _send(), serve(), _validate()

### Community 7 - "Uevent filter tests"
Cohesion: 0.11
Nodes (5): ds4_add(), TEST_F(), uevent(), UeventFeed, fds_

### Community 8 - "main"
Cohesion: 0.13
Nodes (20): backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_UHID, flush_bt_output_for_fd(), hide_method_config_str(), hide_method_is_unbind_family() (+12 more)

### Community 9 - "cli.py"
Cohesion: 0.05
Nodes (37): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+29 more)

### Community 10 - "intercept.c"
Cohesion: 0.07
Nodes (62): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), find_udc(), functionfs_close(), functionfs_init() (+54 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.10
Nodes (27): check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script, status_field(), unit_prop(), test_daemon_ctl.sh script, client() (+19 more)

### Community 12 - "DaemonController (UI)"
Cohesion: 0.05
Nodes (21): DaemonController, m_activeBackend, m_buildError, m_connectionType, m_ds4Backend, m_ds4Name, m_ds4NameIsDefault, m_dualsenseBackend (+13 more)

### Community 13 - "ds4-ctl CLI"
Cohesion: 0.11
Nodes (30): button_names_joined(), ButtonSpec, bit, dpad, name, CommandSpec, arg_completions, name (+22 more)

### Community 14 - "IPC client tests"
Cohesion: 0.08
Nodes (13): config_path(), from_env(), none_sentinel_path(), runtime_dir(), socket_path(), state_dir(), FakeDaemon, dir_ (+5 more)

### Community 15 - "UI IPC client"
Cohesion: 0.09
Nodes (15): IpcClient::closeConnection(), IpcClient::isBusy(), IpcClient::onReadable(), IpcClient::onTimeout(), IpcClient::startNext(), IpcClient, IpcClient::IpcClient(), m_buffer (+7 more)

### Community 16 - "libudev stub tests"
Cohesion: 0.09
Nodes (14): fake_parent(), is_tagged(), physical_ds4(), TEST(), udev_device, devpath, pid, uevent (+6 more)

### Community 17 - "hid-ds4-translator.c"
Cohesion: 0.11
Nodes (9): ds4t_is_emulated(), ds4t_probe(), ds4t_raw_event(), ds4t_read_calibration(), ds4t_read_feature(), ds4t_ring_push(), ds4t_steal(), ds4t_steal_cb() (+1 more)

### Community 18 - "HID-BPF programs"
Cohesion: 0.11
Nodes (8): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG()

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.08
Nodes (25): abort_transport(), cancel_active_transfer(), device_still_present(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx (+17 more)

### Community 20 - "translate.h"
Cohesion: 0.24
Nodes (3): dualshock4_touch_report, points, timestamp

### Community 21 - "Report building & IPC client"
Cohesion: 0.16
Nodes (28): backend_values(), build_led(), build_set_backend(), build_set_name(), connect_socket(), controller_values(), decode_buttons(), dpad_name() (+20 more)

### Community 22 - "scenario_from"
Cohesion: 0.14
Nodes (4): scenario_from(), ParsingTest, VariablesTest, PlanTest

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.08
Nodes (21): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+13 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "FunctionFSDevice"
Cohesion: 0.08
Nodes (22): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+14 more)

### Community 27 - "TrayIcon"
Cohesion: 0.12
Nodes (11): AppController, DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction, m_tray (+3 more)

### Community 28 - "hid-bpf-transport.cpp"
Cohesion: 0.11
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), on_ringbuf_report(), run_udev_hid_bpf() (+8 more)

### Community 29 - "SingleInstance"
Cohesion: 0.21
Nodes (5): SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance, m_server, public

### Community 31 - "daemoncontroller.cpp"
Cohesion: 0.19
Nodes (7): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), isHardwareTest(), TrayIcon::buildMenu()

### Community 32 - "steps"
Cohesion: 0.09
Nodes (21): runs-on, steps, jobs, analyze, name, on, pull_request, push (+13 more)

### Community 33 - "helpers.py"
Cohesion: 0.21
Nodes (3): CliTest, run_cli(), scenario_file()

### Community 34 - "TEST_F"
Cohesion: 0.11
Nodes (4): usb_hid_transport_start(), Clock, TEST_F(), UsbTransportTest

### Community 35 - "lint.yml"
Cohesion: 0.12
Nodes (20): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+12 more)

### Community 36 - "InputState"
Cohesion: 0.10
Nodes (21): decode_touch_point(), InputState, accel, buttons, dpad, gyro, has_motion, has_touch (+13 more)

### Community 37 - "trayicon.cpp"
Cohesion: 0.18
Nodes (3): main(), appletIcon(), TrayIcon::isAvailable()

### Community 38 - "TEST_F"
Cohesion: 0.15
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "libusb_stub.cpp"
Cohesion: 0.18
Nodes (22): usb_hid_transport_close(), usb_hid_transport_open(), Clock, libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_context, libusb_detach_kernel_driver() (+14 more)

### Community 40 - "inputmonitor.cpp"
Cohesion: 0.18
Nodes (7): InputMonitor::clearStates(), InputMonitor::dpadName(), InputMonitor::measure(), InputMonitor::note(), InputMonitor::onReadable(), InputMonitor::tryConnect(), InputMonitor::InputMonitor()

### Community 41 - "dualsense_input_report"
Cohesion: 0.11
Nodes (18): dualsense_input_report, accel, buttons, gyro, points, reserved, reserved2, reserved3 (+10 more)

### Community 42 - "string"
Cohesion: 0.18
Nodes (7): ReadStatus, Again, Data, Eof, Error, LLVMFuzzerTestOneInput(), LLVMFuzzerTestOneInput()

### Community 43 - "RootBroker"
Cohesion: 0.21
Nodes (3): RootBrokerTest, BrokerError, RootBroker

### Community 45 - "DualShock 4 plain layout diagram (SVG, 600x400, Inkscape)"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "gtest"
Cohesion: 0.14
Nodes (7): parse(), Parsed, bus, ok, product, vendor, TEST()

### Community 47 - "libusb_device"
Cohesion: 0.29
Nodes (8): find_hid_interface(), is_own_virtual_usb_device(), libusb_device, libusb_free_config_descriptor(), libusb_get_active_config_descriptor(), libusb_get_bus_number(), libusb_get_device_descriptor(), libusb_get_port_numbers()

### Community 50 - "check_value"
Cohesion: 0.26
Nodes (12): build_set_hide_method(), build_set_mac_squat(), build_set_type(), check_value(), contains(), hide_method_values(), is_valid_hide_method(), is_valid_toggle() (+4 more)

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

### Community 58 - "udev-spoof.c"
Cohesion: 0.31
Nodes (7): init_functions(), is_virtual_sony_controller(), next_uevent_buffer(), udev_device_get_parent_with_subsystem_devtype(), udev_device_get_subsystem(), udev_device_get_sysattr_value(), virtual_mac_serial()

### Community 59 - "jobs"
Cohesion: 0.17
Nodes (12): needs, runs-on, jobs, integration, performance, security-static, smoke-sanity, needs (+4 more)

### Community 60 - "time"
Cohesion: 0.32
Nodes (6): connect(), fuzz(), idle(), raw(), reset(), subscribe()

### Community 61 - "PhysicalNode"
Cohesion: 0.13
Nodes (13): discard_pending_bt_output(), get_event_nodes(), open_and_hide_physical(), PhysicalNode, fd, is_grabbed, is_unlinked, orig_gid (+5 more)

### Community 62 - "Security findings register"
Cohesion: 0.20
Nodes (10): H2 control socket access policy (SO_PEERCRED, ds4-translator group), L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, ds4-ctl runtime control utility, ds4-translator daemon, Test suite (make test targets), ds4-translator-ui Qt target (+2 more)

### Community 63 - "SyntheticState"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

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
Cohesion: 0.18
Nodes (10): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough, Build, hid-ds4-translator (kernel module) (+2 more)

### Community 76 - "Unimplemented DualShock 4 features"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "dualshock4_touch_point"
Cohesion: 0.33
Nodes (6): dualshock4_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 78 - "QStringList"
Cohesion: 0.53
Nodes (5): DaemonController::backendValues(), DaemonController::hideMethodValues(), QStringList, DaemonController::typeValues(), toStringList()

### Community 79 - "Bluetooth HID authorization prompt screenshot"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 80 - "cstring"
Cohesion: 0.17
Nodes (4): set_error(), write_all(), write_file_atomic(), neutral()

### Community 81 - "with"
Cohesion: 0.40
Nodes (5): with, fetch-depth, if-no-files-found, name, path

### Community 83 - "hid-unbind-detect.cpp"
Cohesion: 0.32
Nodes (10): hid_bus_scan_existing(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_open(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+2 more)

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
Cohesion: 0.27
Nodes (8): bound_driver(), check(), hid_dev_by_mac(), nok(), ok(), test-binder.sh script, start_pad(), stop_pads()

### Community 94 - "fake-pad.c"
Cohesion: 0.38
Nodes (8): answer_get_report(), create_device(), handle_events(), main(), send_input(), sleep_ms(), uhid_write(), usage()

### Community 101 - "singleinstance.cpp"
Cohesion: 0.24
Nodes (3): SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance::SingleInstance()

### Community 102 - "dualshock4_input_report_bt"
Cohesion: 0.25
Nodes (8): dualshock4_input_report_bt, common, crc32, num_touch_reports, report_id, reserved, reserved2, touch_reports

### Community 103 - "interrupt_transfer"
Cohesion: 0.38
Nodes (5): interrupt_transfer(), transfer_status_to_error(), libusb_alloc_transfer(), libusb_free_transfer(), libusb_submit_transfer()

### Community 105 - "dualsense_touch_point"
Cohesion: 0.33
Nodes (6): dualsense_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 106 - "dualshock4_input_report_usb"
Cohesion: 0.33
Nodes (6): dualshock4_input_report_usb, common, num_touch_reports, report_id, reserved, touch_reports

### Community 107 - "Debugging and Interceptor Guide"
Cohesion: 0.40
Nodes (5): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), Bluetooth audio output reports

### Community 108 - "StatusField"
Cohesion: 0.40
Nodes (5): parse_status(), status_value(), StatusField, key, value

## Knowledge Gaps
- **344 isolated node(s):** `kSourceCount`, `m_accumulated`, `m_active`, `m_allSources`, `m_buffer` (+339 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 844 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **24 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `daemoncontroller.h`, `TimingStats`, `InputMonitor::disconnectStream`, `InputState`, `OutputState`, `inputmonitor.cpp`, `string`, `Button`, `Source`, `.selected`?**
  _High betweenness centrality (0.069) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `kSourceCount`, `m_accumulated`, `m_active` to the rest of the system?**
  _344 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `hwtest executor tests` be split into smaller, more focused modules?**
  _Cohesion score 0.10285714285714286 - nodes in this community are weakly interconnected._
- **Why does `TEST()` connect `Translation unit tests` to `InputState`, `OutputState`, `string`, `StatusField`, `check_value`, `Report building & IPC client`?**
  _High betweenness centrality (0.048) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `main()` (e.g. with `hid_bus_scan_existing()` and `hid_uevent_monitor_open()`) actually correct?**
  _`main()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._