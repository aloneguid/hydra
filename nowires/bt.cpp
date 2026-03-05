#include "bt.h"
#include "log.h"
#include <stdio.h>
#include <string>
#include <inttypes.h>

using namespace std;

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
    //     if(log_enabled()) log("Waiting for HID Can Send Now...");
    // } 
    uint8_t status = ERROR_CODE_SUCCESS;

    switch(rid) {
        case report_id::kbd:

            if(log_enabled()) log("Keyboard - mod: %02x res: %02x codes (6): %02x / %02x / %02x / %02x / %02x / %02x - mode: %d / id: %d\n",
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
            if(log_enabled()) log("Mouse: %dx%d - buttons: %02x - mode: %d / id: %d", hid_rpt_mouse[1], hid_rpt_mouse[2], hid_rpt_mouse[0], protocol_mode, rid);
            break;

        // case report_id::mouse_abs:
        //     if(protocol_mode == 0) if(log_enabled()) log("Cannot send mouse with absolute positioning in boot mode");
        //     else if(protocol_mode == 1) {
        //         hid_can_send_now = false;
        //         status = hids_device_send_input_report_for_id(central.conn, static_cast<uint16_t>(rid), hid_rpt_mouse_abs, sizeof(hid_rpt_mouse_abs));
        //     }
        //     if(log_enabled()) log("Abs Mouse: %dx%d - buttons: %02x - mode: %d / id: %d", hid_rpt_mouse_abs[1], hid_rpt_mouse_abs[3], hid_rpt_mouse_abs[0], protocol_mode, rid);
        //     hid_can_send_now = false; // reset the flag, we sent the report
        //     break;

        default:
            if(log_enabled()) log("last_report_id == 0, don't know which report to send");
    }

    return status;
}

static void execute_send() {
    hid_central& central = hid_central::current();
    if(!central) {
        hid_rpt_to_send = report_id::none; // reset the report to send
        if(log_enabled()) log("No current central, cannot send report");
        return; // no current central
    }

    send_report(central, hid_rpt_to_send);
    hid_rpt_to_send = report_id::none;
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
            bt::g_bt->update_as();
            if(log_enabled()) {
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
            if(log_enabled()) log("connected %u, addr: %s", conn, bd_addr_to_str(addr));
        }
                                          break;
        case SM_EVENT_JUST_WORKS_REQUEST: {
            if(log_enabled()) log("Just Works requested");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        }
                                        break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST: {
            if(log_enabled()) log("Confirming numeric comparison: %" PRIu32 "", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
        }
                                                break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            if(log_enabled()) log("Display Passkey: %" PRIu32 "", sm_event_passkey_display_number_get_passkey(packet));
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
            if(log_enabled()) {
                log("Identity Resolving Succeeded:");
                log("           addr: %s (%s)", addr_s.c_str(), hid_central::addr_type_to_str(addr_type).c_str());
                log("  resolved addr: %s (%s)", resolved_addr_s.c_str(), hid_central::addr_type_to_str(resolved_addr_type).c_str());
            }

            break;
        }

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            if(log_enabled()) log("L2CAP Connection Parameter Update Complete, response: %x", l2cap_event_connection_parameter_update_response_get_result(packet));
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
                    bt::g_bt->update_as();
                    if(log_enabled()) {
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
                    if(log_enabled()) log("LE Connection Update:");
                    if(log_enabled()) log("- Connection Interval: %u.%02u ms", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    if(log_enabled()) log("- Connection Latency: %u", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                }
                                                               break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_HIDS_META:
            switch(hci_event_hids_meta_get_subevent_code(packet)) {
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    if(log_enabled()) {
                        log("report characteristic subscribed");
                        log("     enable: %u", hids_subevent_input_report_enable_get_enable(packet));
                        log("  report ID: %u", hids_subevent_input_report_enable_get_report_id(packet));
                        log("     handle: %u", hids_subevent_input_report_enable_get_con_handle(packet));
                    }
                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    if(log_enabled()) log("Boot Keyboard Characteristic Subscribed %u", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE:
                    if(log_enabled()) log("Boot Mouse Characteristic Subscribed %u", hids_subevent_boot_mouse_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    if(log_enabled()) log("Protocol Mode: %s mode", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    if(log_enabled()) log("===================HID Can Send Now");
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
                if(log_enabled()) {
                    hci_con_handle_t conn = gap_event_rssi_measurement_get_con_handle(packet);
                    log("RSSI for %uu: %d", conn, rssi);
                }
            }
            break;*/

        default:
            // if(log_enabled()) log("Unhandled HCI event: %02x", hci_event_packet_get_type(packet));
            break;
    }
}

bt* bt::g_bt{nullptr};

bt::bt(app_state& as) : as(as) {
    g_bt = this;
}

void bt::init() {
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
}

void bt::start()  {
    if(hci_power_control(HCI_POWER_ON)) {
        log("Failed to power on Bluetooth");
        return;
    }

    log("Bluetooth initialized and powered on");
}

void bt::adv_toggle() {
    is_advertising = !is_advertising;
    hci_send_cmd(&hci_le_set_advertise_enable, is_advertising ? 1 : 0);
    log("advertising %s", is_advertising ? "enabled" : "disabled");
    as.is_advertising = is_advertising;
}

bool bt::activate_central(uint16_t central_id) {
    bool found = false;
    for(auto& hc : hid_central::centrals()) {
        if(hc.conn == central_id) {
            hid_central::current(hc);
            found = true;
            break;
        }
    }
    return found;
}

void bt::update_as() {
    as.bt_central_count = hid_central::size();
    as.bt_centrals.clear();
    as.bt_centrals_json_array.clear();
    for(hid_central& c: hid_central::centrals()) {
        bool is_active = (c.conn == hid_central::current().conn);
        as.bt_centrals.push_back(app_bt_central{c.conn, is_active, c.addr, hid_central::addr_type_to_str(c.addr_t)});
    }
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


void bt::send_key_press(uint8_t keycode) {
    hid_kbd_rpt_set_keycode(hid_rpt_kbd, keycode);
    submit(report_id::kbd);
    hid_kbd_rpt_set_keycode(hid_rpt_kbd, 0);
    submit(report_id::kbd);
}
