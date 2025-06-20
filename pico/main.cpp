#include <stdio.h>
#include <vector>

// program includes
#include "dashboard.h"
#include "hid.h"
#include "log.h"
#include "binary_serial.h"

// BTstack includes
#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"
#include "device.h" // generated from .gatt by GATT compiler

// PICO specific hardware
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/flash.h"

using namespace std;

#define PRIu32       "u"

//report IDs for the different HID reports.
//must correspond to the HID report map!
enum class report_id : uint16_t {
    none = 0,
    kbd = 1,
    mouse = 2
    // mouse_abs = 5
};

//buffers for the different HID reports
//sizes are determined by the HID report map (look for report count & size)
uint8_t hid_rpt_kbd[8];     // 1 modifier byte, 1 reserved byte, 6 key codes
uint8_t hid_rpt_mouse[4];   // 1 byte for buttons, 1 byte for X, 1 byte for Y, 1 byte for wheel movement
volatile report_id hid_rpt_to_send = report_id::none; // report to send, set by the peripheral handler

// uint8_t hid_rpt_mouse_abs[5]; // 1 byte for buttons, 2 bytes for X, 2 bytes for Y
//currently used report ID, determines the kind of report to be sent after an ATT notification

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 95;
static uint8_t protocol_mode = 1;

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x06, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'H', 'y', 'd', 'r', 'a',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
    // Appearance HID - Keyboard (Category 15, Sub-Category 1)
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC1, 0x03,
};
const uint8_t adv_data_len = sizeof(adv_data);

// HID Report sending

/**
 * Sends report based on the last_report_id (selects which hid_rpt_* to send).
 * The report is sent using the appropriate function based on the protocol mode.
 * If the last_report_id is report_id::none, it does nothing.
 * After sending the report, it resets last_report_id to report_id::none.
 */
static uint8_t send_report(hid_central& central, report_id rid) {

    // log("handle: %u != %u (%s)", central.conn, HCI_CON_HANDLE_INVALID, central.addr.c_str());

    // wait until we can send a report
    // while(!hid_can_send_now) {
    //     if(log_enabled) log("Waiting for HID Can Send Now...");
    // } 
    uint8_t status = ERROR_CODE_SUCCESS;

    switch(rid) {
        case report_id::kbd:

            if(log_enabled) log("Keyboard - mod: %02x res: %02x codes (6): %02x / %02x / %02x / %02x / %02x / %02x - mode: %d / id: %d\n",
                hid_rpt_kbd[0], hid_rpt_kbd[1],
                hid_rpt_kbd[2], hid_rpt_kbd[3], hid_rpt_kbd[4], hid_rpt_kbd[5], hid_rpt_kbd[6], hid_rpt_kbd[7],
                protocol_mode, rid);

            if(protocol_mode == 0) {
                status = hids_device_send_boot_keyboard_input_report(central.conn, hid_rpt_kbd, sizeof(hid_rpt_kbd));
            }
            else if(protocol_mode == 1) {
                status = hids_device_send_input_report_for_id(central.conn, static_cast<uint16_t>(rid), hid_rpt_kbd, sizeof(hid_rpt_kbd));
            }

            break;

        case report_id::mouse:
            if(protocol_mode == 0) {
                status = hids_device_send_boot_mouse_input_report(central.conn, hid_rpt_mouse, sizeof(hid_rpt_mouse));
            }
            else if(protocol_mode == 1) {
                status = hids_device_send_input_report_for_id(central.conn, static_cast<uint16_t>(rid), hid_rpt_mouse, sizeof(hid_rpt_mouse));
            }
            if(log_enabled) log("Mouse: %dx%d - buttons: %02x - mode: %d / id: %d", hid_rpt_mouse[1], hid_rpt_mouse[2], hid_rpt_mouse[0], protocol_mode, rid);
            break;

        // case report_id::mouse_abs:
        //     if(protocol_mode == 0) if(log_enabled) log("Cannot send mouse with absolute positioning in boot mode");
        //     else if(protocol_mode == 1) {
        //         hid_can_send_now = false;
        //         status = hids_device_send_input_report_for_id(central.conn, static_cast<uint16_t>(rid), hid_rpt_mouse_abs, sizeof(hid_rpt_mouse_abs));
        //     }
        //     if(log_enabled) log("Abs Mouse: %dx%d - buttons: %02x - mode: %d / id: %d", hid_rpt_mouse_abs[1], hid_rpt_mouse_abs[3], hid_rpt_mouse_abs[0], protocol_mode, rid);
        //     hid_can_send_now = false; // reset the flag, we sent the report
        //     break;

        default:
            if(log_enabled) log("last_report_id == 0, don't know which report to send");
    }

    return status;
}

static void submit(report_id rid) {
    hid_central& central = hid_central::current();
    if(!central) {
        if(log_enabled) log("No current central, cannot send report");
        return; // no current central
    }

    // request to send a report, this will trigger the HIDS_SUBEVENT_CAN_SEND_NOW event when bluetooth stack is ready
    hid_rpt_to_send = rid; // set the report to send
    uint8_t status = hids_device_request_can_send_now_event(central.conn);
    if(status != ERROR_CODE_SUCCESS) {
        if(log_enabled) log("Error requesting can send now event: %02x", status);
        hid_rpt_to_send = rid; // set the report to send
        return; // error requesting can send now event
    }

    // wait until the report is sent (hid_rpt_to_send is set to report_id::none)
    while(hid_rpt_to_send != report_id::none) {
        if(log_enabled) log("Waiting for report %u to be sent...", static_cast<uint16_t>(rid));
    }

    if(log_enabled) log("Report %u sent", static_cast<uint16_t>(rid));
}

static void execute_send() {
    hid_central& central = hid_central::current();
    if(!central) {
        hid_rpt_to_send = report_id::none; // reset the report to send
        if(log_enabled) log("No current central, cannot send report");
        return; // no current central
    }

    send_report(central, hid_rpt_to_send);
    hid_rpt_to_send = report_id::none;
}

static void send_report111(report_id rid) {

    hid_central& central = hid_central::current();
    if(!central) {
        if(log_enabled) log("No current central, cannot send report");
        return; // no current central
    }

    uint8_t status{0};

    // we'll take our chances to send the report, because most of the time this works,
    // but if buffers are full, we'll retry
    
    while((status = send_report(central, rid)) == BTSTACK_ACL_BUFFERS_FULL) {

        switch(rid) {
            case report_id::kbd:
                dashboard::keyboard_hid_buffers_full++;
                break;
            case report_id::mouse:
                dashboard::mouse_hid_buffers_full++;
                break;
        }

        if(log_enabled) log("buffers full, retrying");
        // wait for a while before retrying
        // sleep_ms(1);
    }

    if(status == ERROR_CODE_SUCCESS) {
        switch(rid) {
            case report_id::kbd:
                dashboard::keyboard_hid_reports_sent++;
                break;
            case report_id::mouse:
                dashboard::mouse_hid_reports_sent++;
                break;
        }
    } else {
        if(log_enabled) log("Error sending report: %02x", status);
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    uint16_t conn_interval;

    if(packet_type != HCI_EVENT_PACKET) return;

    switch(hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            hci_con_handle_t conn = hci_event_disconnection_complete_get_connection_handle(packet);
            hid_central::disconnect(conn);
            if(log_enabled) {
                log("device disconnected:");
                log("  handle: %u", conn);
                uint8_t reason = hci_event_disconnection_complete_get_reason(packet);
                log("  reason: 0x%02x", reason);
            }
        }
                                             break;
        case HCI_EVENT_CONNECTION_COMPLETE: {
            hci_con_handle_t conn = hci_event_connection_complete_get_connection_handle(packet);
            bd_addr_t addr;
            hci_event_connection_complete_get_bd_addr(packet, addr);
            if(log_enabled) log("connected %u, addr: %s", conn, bd_addr_to_str(addr));
        }
                                          break;
        case SM_EVENT_JUST_WORKS_REQUEST: {
            if(log_enabled) log("Just Works requested");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        }
                                        break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST: {
            if(log_enabled) log("Confirming numeric comparison: %" PRIu32 "", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
        }
                                                break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            if(log_enabled) log("Display Passkey: %" PRIu32 "", sm_event_passkey_display_number_get_passkey(packet));
            break;

        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED: {

            // this event occurs even before central is connected, and allows us to map random addresses to public addresses

            bd_addr_t addr;
            sm_event_identity_resolving_succeeded_get_address(packet, addr);
            string addr_s = hid_central::addr_to_str(addr);
            uint8_t addr_type = sm_event_identity_resolving_succeeded_get_addr_type(packet);

            bd_addr_t resolved_addr;
            sm_event_identity_resolving_succeeded_get_identity_address(packet, resolved_addr);
            string resolved_addr_s = hid_central::addr_to_str(resolved_addr);
            uint8_t resolved_addr_type = sm_event_identity_resolving_succeeded_get_identity_addr_type(packet);

            if(addr_type == BD_ADDR_TYPE_LE_RANDOM && resolved_addr_type == BD_ADDR_TYPE_LE_PUBLIC) {
                hid_central::add_address_mapping(addr_s, resolved_addr_s);
            }

            // print address and resolved address and their types
            if(log_enabled) {
                log("Identity Resolving Succeeded:");
                log("           addr: %s (%s)", addr_s.c_str(), hid_central::addr_type_to_str(addr_type).c_str());
                log("  resolved addr: %s (%s)", resolved_addr_s.c_str(), hid_central::addr_type_to_str(resolved_addr_type).c_str());
            }

            break;
        }

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            if(log_enabled) log("L2CAP Connection Parameter Update Complete, response: %x", l2cap_event_connection_parameter_update_response_get_result(packet));
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                    uint8_t addr_type = hci_subevent_le_connection_complete_get_peer_address_type(packet);
                    string addr_type_str = hid_central::addr_type_to_str(addr_type);
                    hci_con_handle_t conn = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    bd_addr_t addr{0};
                    hci_subevent_le_connection_complete_get_peer_address(packet, addr);
                    hid_central hc = hid_central::connect(conn, addr, addr_type);
                    if(log_enabled) {
                        log("LE device connected:");
                        log("  interval: %u.%02u ms", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                        log("   latency: %u", hci_subevent_le_connection_complete_get_conn_latency(packet));
                        log("    handle: %u", conn);
                        log("      addr: %s (%s)", hc.addr.c_str(), addr_type_str.c_str());
                    }
                    
                    if(hid_central::size() < BRPI_MAX_BT_CONNECTIONS) {
                        // keep advertisting if we have space for more devices
                        hci_send_cmd(&hci_le_set_advertise_enable, 1);
                        // hci_send_cmd(&hci_le_set_scan_enable, 1);
                        log("       dev: %u < %u", hid_central::size(), BRPI_MAX_BT_CONNECTIONS);
                    }
                }
                                                        break;
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE: {
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                    if(log_enabled) log("LE Connection Update:");
                    if(log_enabled) log("- Connection Interval: %u.%02u ms", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    if(log_enabled) log("- Connection Latency: %u", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                }
                                                               break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_HIDS_META:
            switch(hci_event_hids_meta_get_subevent_code(packet)) {
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    if(log_enabled) {
                        log("report characteristic subscribed");
                        log("     enable: %u", hids_subevent_input_report_enable_get_enable(packet));
                        log("  report ID: %u", hids_subevent_input_report_enable_get_report_id(packet));
                        log("     handle: %u", hids_subevent_input_report_enable_get_con_handle(packet));
                    }
                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    if(log_enabled) log("Boot Keyboard Characteristic Subscribed %u", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE:
                    if(log_enabled) log("Boot Mouse Characteristic Subscribed %u", hids_subevent_boot_mouse_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    if(log_enabled) log("Protocol Mode: %s mode", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    if(log_enabled) log("===================HID Can Send Now");
                    // on_hid_can_send_now();
                    execute_send();
                    break;
                default:
                    break;
            }
            break;

        // response to hci_send_cmd(&hci_read_rssi, central.conn);
        /*case GAP_EVENT_RSSI_MEASUREMENT: {
                uint8_t rssi = gap_event_rssi_measurement_get_rssi(packet);
                if(log_enabled) {
                    hci_con_handle_t conn = gap_event_rssi_measurement_get_con_handle(packet);
                    log("RSSI for %uu: %d", conn, rssi);
                }
            }
            break;*/

        default:
            // if(log_enabled) log("Unhandled HCI event: %02x", hci_event_packet_get_type(packet));
            break;
    }
}

bool peripherial_handler(uint8_t cmd) {
    if(cmd == 1) {
        // keyboard, read 8 more bytes
        if(log_enabled) log("keyboard command");
        stdin_buf(hid_rpt_kbd, sizeof(hid_rpt_kbd));
        submit(report_id::kbd);
        return true;
    } else if(cmd == 2) {
        // mouse, read 4 more bytes
        if(log_enabled) log("mouse command");
        stdin_buf(hid_rpt_mouse, sizeof(hid_rpt_mouse));
        submit(report_id::mouse);
        return true;
    }
    return false;
}

void emulation_handler() {
    if(!hid_central::any()) {
        if(log_enabled) log("no HID devices connected");
        return;
    }

    printf("  t - type 't' on the keyboard\n");
    printf("  w,a,s,d - mouse movements\n");
    printf("  m - mouse latency test (100 reports)\n");
    printf("  x - exit emulation\n");

    while(true) {
        uint8_t cmd = stdin_uint8();
        if(cmd == 'x') {
            printf("exit\n");
            break;
        }

        printf(".");

        if(cmd == 't') {
            // press 't'
            hid_kbd_rpt_set_keycode(hid_rpt_kbd, 0x17);
            submit(report_id::kbd);
            hid_kbd_rpt_set_keycode(hid_rpt_kbd, 0);
            submit(report_id::kbd);
        } else if(cmd == 'w' || cmd == 'a' || cmd == 's' || cmd == 'd') {

            // mouse movement
            hid_rpt_mouse[0] = 0; // buttons pressed
            hid_rpt_mouse[1] = 0; // X movement
            hid_rpt_mouse[2] = 0; // Y movement
            hid_rpt_mouse[3] = 0; // wheel movement

            const uint8_t speed = 10;

            if(cmd == 'w') hid_rpt_mouse[2] = -speed; // Y movement up
            else if(cmd == 'a') hid_rpt_mouse[1] = -speed; // X movement left
            else if(cmd == 's') hid_rpt_mouse[2] = speed; // Y movement down
            else if(cmd == 'd') hid_rpt_mouse[1] = speed; // X movement right

            submit(report_id::mouse);

        } else if(cmd == 'm') {
            // mouse movement
            hid_rpt_mouse[0] = 0; // buttons pressed
            hid_rpt_mouse[1] = 0; // X movement
            hid_rpt_mouse[2] = 1; // Y movement
            hid_rpt_mouse[3] = 0; // wheel movement

            for(int i = 0; i < 100; i++) {
                int64_t start = time_us_64();
                submit(report_id::mouse);
                // sleep_ms(5);
                int64_t end = time_us_64();
                int64_t elapsed_ms = (end - start) / 1000;
                printf("elapsed: %lld ms\n", elapsed_ms);
            }

        } else {
            printf("unknown command: %c\n", cmd);
        }
    }
}

bool interactive_handler(uint8_t cmd) {

    if(cmd == '?') {
        printf("available commands:\n");
        printf("  l - list centrals\n");
        printf("  s - switch central, followed by it's address\n");
        printf("  u - unpair from all devices\n");
        printf("  a - advertising, followed by 'b' to begin or 'e' to end\n");
        printf("  e - emulation mode (for testing)\n");
        printf("  d - dashboard, followed by 's' to show or 'r' to reset\n");
        printf("  v - toggle verbosity (logging)\n");
        printf("  r - restart the device\n");
        printf("  ? - print this help\n");
        return true;
    }

    if(cmd == 'l') {
        printf("#\tHANDLE\tADDRESS\t\t\tAT\tIRK\n");
        int index = 0;
        for(hid_central& central : hid_central::centrals()) {
            string marker = (central.conn == hid_central::current().conn) ? "*" : " ";
            printf("%u %s\t%u\t%s\t%s\t%s\n", index++,
                marker.c_str(),
                central.conn,
                central.addr.c_str(),
                hid_central::addr_type_to_str(central.addr_t).c_str(),
                central.irk.c_str());

            // todo: remove next line
            // hci_send_cmd(&hci_read_rssi, central.conn);
        }

        // print_ble_devices();
        return true;
    }

    if(cmd == 's') {
        string address;
        printf("address: ");
        while(address.size() < 17) { // 17 = 6 bytes + 5 colons
            char c = getchar();
            printf("%c", c);
            address += c;
        }
        printf("\n");

        bool found = false;
        for(auto& hc : hid_central::centrals()) {
            if(hc.addr == address) {
                hid_central::current(hc);
                found = true;
                if(log_enabled) log("switched to %s", address.c_str());
                break;
            }
        }

        if(!found) {
            if(log_enabled) log("no device with address %s found", address.c_str());
        }

        return true;
    }

    if(cmd == 'a') {
        uint8_t subcmd = stdin_uint8();
        bool enable = (subcmd == 'b');
        uint8_t r = hci_send_cmd(&hci_le_set_advertise_enable, enable);
        if(enable) {
            printf("advertising begun\n");
        } else {
            printf("advertising ended\n");
        }
        return true;
    }

    if(cmd == 'e') {
        emulation_handler();
        return true;
    }

    if(cmd == 'd') {
        uint8_t subcmd = stdin_uint8();
        if(subcmd == 's') {
            if(log_enabled) log("showing dashboard");
            dashboard::print_stats();
        } else if(subcmd == 'r') {
            if(log_enabled) log("resetting dashboard");
            dashboard::reset_stats();
            dashboard::print_stats();
        } else {
            if(log_enabled) log("unknown dashboard command: %c", subcmd);
        }
        return true;
    }

    if(cmd == 'v') {
        log_enabled = !log_enabled;
        if(log_enabled) log("logging enabled");
        else log("logging disabled");
        return true;
    }

    if(cmd == 'u') {

        // remove all devices from btstack storage
        if(log_enabled) log("unpairing from all devices...");
        for(int i = 0; i < le_device_db_max_count(); i++) {
            if(log_enabled) log("removing device %d", i);
            le_device_db_remove(i);
        }

        if(log_enabled) log("restarting...\n");
        watchdog_reboot(0, 0, 0);
        return true;
    }

    if(cmd == 'r') {
        if(log_enabled) log("restarting...\n");
        watchdog_reboot(0, 0, 0);
    }

    return false;
}

bool interactive_handler_with_led(uint8_t cmd) {
    dashboard::led_put(true);
    bool result = interactive_handler(cmd);
    dashboard::led_put(false);
    return result;
}

void protocol_handler() {

    if(log_enabled) log("HID Protocol Handler started.");

    while(true) {
        // handle protocol events
        uint8_t cmd = stdin_uint8();
        if(log_enabled) log("command: %d", cmd);

        if(!(peripherial_handler(cmd) || interactive_handler_with_led(cmd))) {
            if(log_enabled) log("unknown command: %d", cmd);
            dashboard::led_blink(10, 30); // blink to indicate error
        }
    }
}

int main() {
    stdio_init_all();

    // initialize CYW43 driver architecture (will enable BT if/because CYW43_ENABLE_BLUETOOTH == 1)
    if(cyw43_arch_init()) {
        if(log_enabled) log("failed to initialise cyw43_arch");
        return -1;
    }

    l2cap_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup battery service
    battery_service_server_init(battery);

    // setup device information service
    device_information_service_server_init();

    // setup HID Device service
    hids_device_init(44, HidReportMap, sizeof(HidReportMap));

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*)adv_data);
    gap_advertisements_enable(1);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // register for HIDS
    hids_device_register_packet_handler(packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    // blink LED 5 times to indicate startup
    dashboard::led_blink(5, 20);

    // interactive_test();
    protocol_handler();

    return 0;
}