# Graph Report - ds4-translator  (2026-11-22)

## Corpus Check
- 107 files · ~120,814 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 31 file(s) not represented in the graph (top: .qml 12, (none) 5, .toml 5)
- WARNING: 31 of 138 files seen (22%) have no supported extractor and are missing from this graph entirely. It may look complete while representing only part of the codebase.

## Summary
- 1882 nodes · 3634 edges · 101 communities (82 shown, 19 thin omitted)
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 384 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- hwtest executor tests
- Qt UI model (DS4 settings)
- Daemon system headers & udev/hidraw
- Translation unit tests
- DS4 report decoding & mapping
- Release workflow (CI)
- hwtest runtime (process/broker)
- Uevent filter tests
- Emulation backends
- hwtest scenario model
- Daemon main loop & IPC socket
- Hardware verify.sh
- DaemonController (UI)
- ds4-ctl CLI
- IPC client tests
- UI IPC client
- libudev stub tests
- hwtest CLI
- HID-BPF programs
- USB HID transport (libusb)
- Descriptors & touchpad
- Report building & IPC client
- FunctionFS gadget
- Bluetooth output pacer
- AppController (UI)
- Community 25
- Community 27
- Community 28
- Community 29
- Community 30
- Community 31
- Community 32
- Community 33
- Community 34
- Community 35
- Community 36
- Community 37
- Community 38
- Community 39
- Community 40
- Community 41
- Community 42
- Community 43
- Community 44
- Community 45
- Community 46
- Community 47
- Community 48
- Community 49
- Community 50
- Community 51
- Community 52
- Community 53
- Community 54
- Community 55
- Community 56
- Community 57
- Community 58
- Community 59
- Community 60
- Community 61
- Community 62
- Community 63
- Community 64
- Community 66
- Community 67
- Community 68
- Community 69
- Community 70
- Community 71
- Community 72
- Community 73
- Community 74
- Community 75
- Community 76
- Community 77
- Community 78
- Community 79
- Community 80
- Community 81
- Community 82
- Community 83
- Community 84
- Community 85
- Community 86
- Community 87
- Community 88
- Community 89
- Community 90
- Community 91
- Community 92
- Community 93
- Community 94
- Community 95
- Community 96
- Community 97
- Community 98
- Community 99
- Community 100

## God Nodes (most connected - your core abstractions)
1. `InputMonitor` - 109 edges
2. `TEST()` - 87 edges
3. `DaemonController` - 73 edges
4. `main()` - 54 edges
5. `TEST()` - 47 edges
6. `close()` - 38 edges
7. `AppController` - 38 edges
8. `Result` - 33 edges
9. `TransportState` - 32 edges
10. `run_scenario()` - 31 edges

## Surprising Connections (you probably didn't know these)
- `TEST()` --calls--> `dpad_name()`  [INFERRED]
  tests/unit/test_ipc_protocol.cpp → src/ipc-client.cpp
- `run_virtual_ui()` --calls--> `poll()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c
- `run_virtual_ui()` --calls--> `read()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c
- `run_test_ui()` --calls--> `close()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c
- `run_test_ui()` --calls--> `poll()`  [INFERRED]
  src/ctl.cpp → tools/ds4-intercept/intercept.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Mechanisms for hiding the Bluetooth physical pad** — docs_how_it_works_hide_methods, docs_how_it_works_hid_generic_swap, docs_how_it_works_mac_squat, docs_how_it_works_unlink_nodes [EXTRACTED 1.00]
- **Arch package release and signing pipeline** — packaging_arch_readme_pacman_repository, packaging_arch_readme_arch_repo_ci, packaging_arch_readme_signing, packaging_arch_index_pacman_repo_page [EXTRACTED 1.00]

## Communities (101 total, 19 thin omitted)

### Community 0 - "hwtest executor tests"
Cohesion: 0.05
Nodes (18): FakeRunner, FakeUI, scenario_from(), CliTest, run_cli(), scenario_file(), ParsingTest, VariablesTest (+10 more)

### Community 1 - "Qt UI model (DS4 settings)"
Cohesion: 0.03
Nodes (21): InputMonitor, kSourceCount, m_accumulated, m_active, m_allSources, m_buffer, m_connectionNote, m_fd (+13 more)

### Community 2 - "Daemon system headers & udev/hidraw"
Cohesion: 0.07
Nodes (41): device_node_sysfs_path(), discard_pending_bt_output(), find_physical_ds4(), find_usb_ds4_hid_id(), get_event_nodes(), hidraw_to_hid_id(), is_own_virtual_hidraw(), kick_bt_full_report_mode() (+33 more)

### Community 4 - "DS4 report decoding & mapping"
Cohesion: 0.06
Nodes (19): functionfs_send_event_packet(), emit_input_report(), send_gip_guide_event(), BatteryState, BATTERY_CHARGING, BATTERY_DISCHARGING, BATTERY_FULL, build_gip_input_report() (+11 more)

### Community 5 - "Release workflow (CI)"
Cohesion: 0.05
Nodes (45): container, needs, permissions, runs-on, steps, needs, runs-on, steps (+37 more)

### Community 6 - "hwtest runtime (process/broker)"
Cohesion: 0.09
Nodes (9): serve_lines(), ServeTest, StreamProcessTest, BadRequest, _execute(), _send(), serve(), _validate() (+1 more)

### Community 7 - "Uevent filter tests"
Cohesion: 0.07
Nodes (16): hid_bus_scan_existing(), hid_id_env_matches_ds4(), hid_id_sysfs_matches_ds4(), hid_id_unbind(), hid_uevent_monitor_open(), hid_uevent_monitor_read(), hid_uevent_sender_is_kernel(), is_ds4_vid_pid_bus() (+8 more)

### Community 8 - "Emulation backends"
Cohesion: 0.08
Nodes (40): functionfs_set_mac(), backend_for_type(), backend_type_config_str(), backend_type_name(), BackendType, BACKEND_FUNCTIONFS, BACKEND_UHID, create_virtual_device() (+32 more)

### Community 9 - "hwtest scenario model"
Cohesion: 0.13
Nodes (10): count_steps(), Step, substitute(), Executor, hit(), probe(), Outcome, Runner (+2 more)

### Community 10 - "Daemon main loop & IPC socket"
Cohesion: 0.15
Nodes (33): background_delayed_udevadm_trigger(), read_hid_ignore_special_drivers(), setup_ipc_socket(), write_hid_ignore_special_drivers(), pump_thread_fn(), classify_path(), close(), decode_evdev_ioctl() (+25 more)

### Community 11 - "Hardware verify.sh"
Cohesion: 0.12
Nodes (30): check_virtual_node(), find_virtual_hidraw(), sandbox_errors(), verify.sh script, status_field(), unit_prop(), test_daemon_ctl.sh script, bad_utf8_rejected() (+22 more)

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

### Community 17 - "hwtest CLI"
Cohesion: 0.11
Nodes (13): build_parser(), builtin_variables(), cmd_check(), cmd_list(), cmd_run(), load_scenario(), main(), resolve() (+5 more)

### Community 18 - "HID-BPF programs"
Cohesion: 0.11
Nodes (8): extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG(), extract_bits(), field_end_byte(), field_start_byte(), BPF_PROG()

### Community 19 - "USB HID transport (libusb)"
Cohesion: 0.08
Nodes (21): abort_transport(), on_transfer_done(), TransportState, active_transfer, bus_number, ctx, device_address, ep_in (+13 more)

### Community 20 - "Descriptors & touchpad"
Cohesion: 0.07
Nodes (23): dualsense_touch_point, contact, x_hi, x_lo, y_hi, y_lo, dualshock4_input_report_bt, common (+15 more)

### Community 21 - "Report building & IPC client"
Cohesion: 0.17
Nodes (25): build_led(), build_set_mac_squat(), connect_socket(), decode_buttons(), dpad_name(), errno_text(), format_state_line(), format_timing_line() (+17 more)

### Community 22 - "FunctionFS gadget"
Cohesion: 0.12
Nodes (20): build_feature_report(), build_ffs_descriptors(), build_ffs_strings(), ensure_configfs_mounted(), ep0_loop(), functionfs_close(), functionfs_init(), functionfs_send_input_report() (+12 more)

### Community 23 - "Bluetooth output pacer"
Cohesion: 0.09
Nodes (17): BtOutputPacer, cond_, kMinInterval, last_write_, mutex_, pending_, pending_fd_, pending_state_ (+9 more)

### Community 24 - "AppController (UI)"
Cohesion: 0.08
Nodes (8): AppController, m_autostartEnabled, m_closeToTray, m_settingsError, m_startHidden, m_trayAvailable, m_trayEnabled, QML_ELEMENT

### Community 25 - "Community 25"
Cohesion: 0.08
Nodes (22): FunctionFSDevice, configured, device_open, ep0_fd, ep0_thread, ep0_thread_spawned, ep_in_fd, ep_out_fd (+14 more)

### Community 27 - "Community 27"
Cohesion: 0.11
Nodes (12): AppController, TrayIcon::buildMenu(), DaemonController, TrayIcon, m_app, m_daemon, m_menu, m_summaryAction (+4 more)

### Community 28 - "Community 28"
Cohesion: 0.13
Nodes (16): bpf_object_path(), find_hidraw_node(), find_pinned_ringbuf(), hid_bpf_transport_close(), hid_bpf_transport_open(), log_write_error(), pump_thread_fn(), run_udev_hid_bpf() (+8 more)

### Community 29 - "Community 29"
Cohesion: 0.12
Nodes (8): SingleInstance::listen(), SingleInstance::notifyRunningInstance(), SingleInstance::socketPath(), ensurePrivateDirectory(), SingleInstance, m_server, public, SingleInstance::SingleInstance()

### Community 31 - "Community 31"
Cohesion: 0.21
Nodes (6): DaemonController::beginInteraction(), DaemonController::endInteraction(), DaemonController::identify(), DaemonController::refreshStatus(), DaemonController::resetLed(), isHardwareTest()

### Community 32 - "Community 32"
Cohesion: 0.09
Nodes (21): runs-on, steps, jobs, analyze, name, on, pull_request, push (+13 more)

### Community 33 - "Community 33"
Cohesion: 0.19
Nodes (13): _argv(), check_variables(), texts(), visit(), _expand_steps(), load(), _number(), _parse_guard() (+5 more)

### Community 34 - "Community 34"
Cohesion: 0.11
Nodes (4): usb_hid_transport_start(), Clock, TEST_F(), UsbTransportTest

### Community 35 - "Community 35"
Cohesion: 0.12
Nodes (20): runs-on, steps, jobs, clang-tidy, lint, python, runs-on, steps (+12 more)

### Community 36 - "Community 36"
Cohesion: 0.10
Nodes (21): decode_touch_point(), InputState, accel, buttons, dpad, gyro, has_motion, has_touch (+13 more)

### Community 37 - "Community 37"
Cohesion: 0.12
Nodes (4): main(), appletIcon(), TrayIcon::isAvailable(), TrayIcon::TrayIcon()

### Community 38 - "Community 38"
Cohesion: 0.12
Nodes (3): AtomicWrite, dir_, TEST_F()

### Community 39 - "Community 39"
Cohesion: 0.17
Nodes (13): interrupt_transfer(), transfer_status_to_error(), Clock, libusb_alloc_transfer(), libusb_cancel_transfer(), libusb_context, libusb_error_name(), libusb_exit() (+5 more)

### Community 40 - "Community 40"
Cohesion: 0.18
Nodes (7): InputMonitor::clearStates(), InputMonitor::dpadName(), InputMonitor::measure(), InputMonitor::note(), InputMonitor::onReadable(), InputMonitor::tryConnect(), InputMonitor::InputMonitor()

### Community 41 - "Community 41"
Cohesion: 0.11
Nodes (18): dualsense_input_report, accel, buttons, gyro, points, reserved, reserved2, reserved3 (+10 more)

### Community 42 - "Community 42"
Cohesion: 0.22
Nodes (4): StatusField, key, value, le16()

### Community 43 - "Community 43"
Cohesion: 0.21
Nodes (3): RootBrokerTest, BrokerError, RootBroker

### Community 45 - "Community 45"
Cohesion: 0.16
Nodes (16): Controller body outline (g3285), D-pad (g3242 / clipPath3233), Face buttons cross/circle/square/triangle (g3229 group), Dot cluster glyphs (g3982, path3864 series), Inkscape 0.48.1 origin, docname 'Dualshock 4 plain Layout', Vector-path text labels (text4575, text4575-2), DualShock 4 plain layout diagram (SVG, 600x400, Inkscape), Left analog stick (g3262) (+8 more)

### Community 46 - "Community 46"
Cohesion: 0.13
Nodes (9): parse_hid_id(), LLVMFuzzerTestOneInput(), parse(), Parsed, bus, ok, product, vendor (+1 more)

### Community 47 - "Community 47"
Cohesion: 0.24
Nodes (14): device_still_present(), find_hid_interface(), is_own_virtual_usb_device(), usb_hid_transport_open(), libusb_device, libusb_free_config_descriptor(), libusb_free_device_list(), libusb_get_active_config_descriptor() (+6 more)

### Community 49 - "Community 49"
Cohesion: 0.16
Nodes (3): set_error(), write_all(), write_file_atomic()

### Community 50 - "Community 50"
Cohesion: 0.22
Nodes (14): backend_values(), build_set_backend(), build_set_hide_method(), build_set_name(), check_value(), contains(), controller_values(), hide_method_values() (+6 more)

### Community 51 - "Community 51"
Cohesion: 0.13
Nodes (15): Button, kBtnCircle, kBtnCross, kBtnL1, kBtnL2, kBtnL3, kBtnOptions, kBtnPs (+7 more)

### Community 52 - "Community 52"
Cohesion: 0.13
Nodes (15): dualshock4_input_report_common, accel, buttons, gyro, reserved2, reserved3, rx, ry (+7 more)

### Community 53 - "Community 53"
Cohesion: 0.13
Nodes (15): Button, Circle, Cross, L1, L2, L3, Options, Ps (+7 more)

### Community 54 - "Community 54"
Cohesion: 0.15
Nodes (12): concurrency, cancel-in-progress, group, name, on, pull_request, push, workflow_call (+4 more)

### Community 55 - "Community 55"
Cohesion: 0.36
Nodes (13): steps, steps, steps, steps, steps, steps, steps, continue-on-error (+5 more)

### Community 56 - "Community 56"
Cohesion: 0.17
Nodes (12): Shoulder bumpers L1/R1, DualShock 4 controller, DualShock 4 base layout diagram (unlabelled SVG), D-pad, Face buttons (cross, circle, square, triangle), Light bar, PS button, Share and Options buttons (+4 more)

### Community 57 - "Community 57"
Cohesion: 0.20
Nodes (6): log_physical_write_error(), write_physical_output_report(), calculate_crc32(), ns_per_op(), report(), TEST()

### Community 58 - "Community 58"
Cohesion: 0.27
Nodes (7): init_functions(), is_virtual_sony_controller(), next_uevent_buffer(), udev_device_get_parent_with_subsystem_devtype(), udev_device_get_subsystem(), udev_device_get_sysattr_value(), virtual_mac_serial()

### Community 59 - "Community 59"
Cohesion: 0.17
Nodes (12): needs, runs-on, jobs, integration, performance, security-static, smoke-sanity, needs (+4 more)

### Community 60 - "Community 60"
Cohesion: 0.32
Nodes (6): connect(), fuzz(), idle(), raw(), reset(), subscribe()

### Community 61 - "Community 61"
Cohesion: 0.29
Nodes (12): cancel_active_transfer(), usb_hid_transport_close(), libusb_attach_kernel_driver(), libusb_claim_interface(), libusb_close(), libusb_detach_kernel_driver(), libusb_device_handle, pending (+4 more)

### Community 62 - "Community 62"
Cohesion: 0.18
Nodes (11): Bluetooth HID session teardown (BlueZ send buffer overrun), Debugging and Interceptor Guide, ds4-ctl status diagnostics, libds4-intercept (LD_PRELOAD tracer), H2 control socket access policy (SO_PEERCRED, ds4-translator group), Bluetooth audio output reports, ds4-ctl runtime control utility, ds4-translator daemon (+3 more)

### Community 63 - "Community 63"
Cohesion: 0.20
Nodes (11): build_synthetic_common(), emit_neutral_report(), SyntheticState, buttons, dpad, l2_analog, lx, ly (+3 more)

### Community 64 - "Community 64"
Cohesion: 0.27
Nodes (10): hid-generic swap, Hide methods (legacy/unbind/unlink), MAC squat, Unlink of /dev nodes and reapply sweep, Workaround stack the module would replace, Physical controller hiding, hid-bpf-probe feasibility spike, HID-BPF hid_device_event swallow and ringbuf mirror (+2 more)

### Community 66 - "Community 66"
Cohesion: 0.20
Nodes (9): TimingStats, interval_max_us, interval_mean_us, interval_min_us, latency_max_us, latency_mean_us, reports, window_us (+1 more)

### Community 67 - "Community 67"
Cohesion: 0.39
Nodes (8): find_udc(), closedir(), get_tracked_dir(), is_interesting_dir(), opendir(), readdir(), track_dir(), untrack_dir()

### Community 68 - "Community 68"
Cohesion: 0.22
Nodes (8): description, name, runs, steps, using, name, run, shell

### Community 70 - "Community 70"
Cohesion: 0.25
Nodes (8): needs, runs-on, strategy, compatibility, compiler, os, fail-fast, matrix

### Community 71 - "Community 71"
Cohesion: 0.32
Nodes (8): Backend selection (uhid vs functionfs), Known issues (Steam MAC, BT auth popup), ds4-translator README, USB priority live hot-swap, Xbox 360/One/Series emulation, hwtest guided hardware test runner, Single pkexec root helper, hwtest TOML scenario format

### Community 72 - "Community 72"
Cohesion: 0.25
Nodes (8): format_output_line(), OutputState, blue, green, red, rumble_left, rumble_right, parse_output()

### Community 73 - "Community 73"
Cohesion: 0.33
Nodes (7): FunctionFS backend, How DS4-Translator Works, Haptics rumble heuristic, Emulation type none, udev hide rules (72-ds4-translator-hide.rules), Virtual controller identity and feature reports, Light bar flash passthrough

### Community 74 - "Community 74"
Cohesion: 0.33
Nodes (5): Controls, feed, gone, reclaimed, wedge

### Community 75 - "Community 75"
Cohesion: 0.33
Nodes (6): Alternatives to out-of-tree module, Pad character device interface, Kernel module costs and risks (DKMS, Secure Boot, GPL), hid-ds4-translator kernel module (Option A binder), Design study: ds4-translator kernel module, IMU calibration passthrough

### Community 76 - "Community 76"
Cohesion: 0.40
Nodes (6): Short BT input report 0x01 fallback, Cable pairing (feature 0x13), dualshock-tools/ds4-tools, PS4 Developer wiki DS4-BT, Stick and trigger calibration (ds4-tools), Unimplemented DualShock 4 features

### Community 77 - "Community 77"
Cohesion: 0.33
Nodes (6): dualshock4_touch_point, contact, x_hi, x_lo, y_hi, y_lo

### Community 78 - "Community 78"
Cohesion: 0.53
Nodes (5): DaemonController::backendValues(), DaemonController::hideMethodValues(), QStringList, DaemonController::typeValues(), toStringList()

### Community 79 - "Community 79"
Cohesion: 0.50
Nodes (5): Buttons: Always accept, Accept, Deny, DS4 controller Bluetooth pairing authorization step, Human Interface Device Service (HID), Bluetooth HID authorization prompt screenshot, Wireless Controller (74:E7:D6:3A:47:E8)

### Community 80 - "Community 80"
Cohesion: 0.40
Nodes (5): L5 systemd service sandboxing, M3 USB watchdog use-after-free, Security findings register, Daemon strace syscall profile, Test suite (make test targets)

### Community 81 - "Community 81"
Cohesion: 0.40
Nodes (5): with, fetch-depth, if-no-files-found, name, path

### Community 83 - "Community 83"
Cohesion: 0.40
Nodes (5): ReadStatus, Again, Data, Eof, Error

### Community 85 - "Community 85"
Cohesion: 0.50
Nodes (4): container, needs, runs-on, compatibility-arch

### Community 86 - "Community 86"
Cohesion: 0.50
Nodes (4): pacman repository landing page, arch-repo release CI job and GitHub Pages deploy, Arch pacman repository setup, Package signing with release subkey

### Community 88 - "Community 88"
Cohesion: 0.50
Nodes (3): Source, Physical, Virtual

### Community 90 - "Community 90"
Cohesion: 0.67
Nodes (3): security, needs, runs-on

### Community 91 - "Community 91"
Cohesion: 0.67
Nodes (3): unit, needs, runs-on

### Community 92 - "Community 92"
Cohesion: 0.67
Nodes (3): build_set_type(), is_valid_type(), type_values()

## Knowledge Gaps
- **344 isolated node(s):** `name`, `bit`, `dpad`, `name`, `arg_completions` (+339 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 820 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **19 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `InputMonitor` connect `Qt UI model (DS4 settings)` to `Community 65`, `Community 66`, `Community 99`, `Community 36`, `Community 72`, `Community 40`, `Community 53`, `Community 88`, `Community 26`?**
  _High betweenness centrality (0.094) - this node is a cross-community bridge._
- **Are the 34 inferred relationships involving `TEST()` (e.g. with `backend_values()` and `build_led()`) actually correct?**
  _`TEST()` has 34 INFERRED edges - model-reasoned connections that need verification._
- **What connects `name`, `bit`, `dpad` to the rest of the system?**
  _344 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `hwtest executor tests` be split into smaller, more focused modules?**
  _Cohesion score 0.052434456928838954 - nodes in this community are weakly interconnected._
- **Why does `DaemonController` connect `DaemonController (UI)` to `Community 65`, `Community 44`, `Community 78`, `UI IPC client`, `Community 31`, `Community 95`?**
  _High betweenness centrality (0.049) - this node is a cross-community bridge._
- **Are the 17 inferred relationships involving `main()` (e.g. with `hid_bus_scan_existing()` and `hid_uevent_monitor_open()`) actually correct?**
  _`main()` has 17 INFERRED edges - model-reasoned connections that need verification._
- **Should `Qt UI model (DS4 settings)` be split into smaller, more focused modules?**
  _Cohesion score 0.03278688524590164 - nodes in this community are weakly interconnected._