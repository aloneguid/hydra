#include "machine.h"
#include "ui/control.h"
#include "win32/clipboard.h"
#include "win32/shell.h"
#include <fmt/core.h>
#include "hid.h"
#include <thread>
#include <filesystem>

// {3855BE6C-A922-407C-8BA8-96199B00667D}
static const GUID ShellIcon =
    {0x3855be6c, 0xa922, 0x407c, { 0x8b, 0xa8, 0x96, 0x19, 0x9b, 0x0, 0x66, 0x7d }};
#define OWN_WM_NOTIFY_ICON_MESSAGE WM_APP + 1
#define IconDisconnected L"IDI_ICON1"
#define IconConnected L"IDI_S_CONNECTED"
#define IconActivityKeyboard L"IDI_S_ACT_KBD"
#define IconActivityMouse L"IDI_S_ACT_MOUSE"

namespace hydra {

    using namespace std;
    namespace fs = std::filesystem;

    void machine::notify(const std::string& text) {
        win32sni.display_notification(APP_LONG_NAME, text);
    }

    void machine::on_key_event(const key_event& e) {
        auto vk_it = std::find(vk_down.begin(), vk_down.end(), e.vk_code);
        if(e.down) {
            if(vk_it == vk_down.end()) {
                vk_down.push_back(e.vk_code);
            }
        } else {
            if(vk_it != vk_down.end()) {
                vk_down.erase(vk_it);
            }
        }

        if(is_remoting) {
            win32sni.update_icon(IconActivityKeyboard);
            dongle.send_kbd(vk_down);
        }
    }

    void machine::on_mouse_event(const mouse_event& e) {

        if(!m_initialised) {
            m_x = e.x;
            m_y = e.y;
            m_xdiff = 0;
            m_ydiff = 0;
            m_wheel_delta = 0;
            m_initialised = true;
        } else {

            // calculate mouse movement
            m_xdiff = e.x - m_x;
            m_ydiff = e.y - m_y;
            m_wheel_delta = e.wheel_delta;

            // clamp to 127
            if(m_xdiff > 127) m_xdiff = 127;
            else if(m_xdiff < -127) m_xdiff = -127;
            if(m_ydiff > 127) m_ydiff = 127;
            else if(m_ydiff < -127) m_ydiff = -127;
        }

        if(e.button != mouse_button::none) {
            switch(e.button) {
                case mouse_button::left:
                    m_l = e.down;
                    break;
                case mouse_button::right:
                    m_r = e.down;
                    break;
                case mouse_button::middle:
                    m_m = e.down;
                    break;
            }
        }

#if _DEBUG
        ::OutputDebugStringA(
            fmt::format("mouse state. x: {}->{}, y: {}->{}, x_diff: {}, y_diff: {}\n",
                m_x, e.x, m_y, e.y, m_xdiff, m_ydiff)
            .c_str());

#endif

        // measure performance of this call
        //auto last_time = std::chrono::steady_clock::now();
        float multiplier = 0.01f * mouse_sensitivity;
        win32sni.update_icon(IconActivityMouse);
        dongle.send_mouse(m_l, m_m, m_r,
            m_xdiff * multiplier,
            m_ydiff * multiplier, m_wheel_delta);
        //auto now = std::chrono::steady_clock::now();
        //auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
        //::OutputDebugStringA(
        //    fmt::format("mouse sent in {} ms\n", elapsed).c_str());
    }

    machine::machine() : 
        win32sni{win32app.get_hwnd(), ShellIcon, OWN_WM_NOTIFY_ICON_MESSAGE, "KM"},
        win32pm{win32app.get_hwnd()},
        cfg{get_config_file_path()} {

        try {
            dongle.refresh();
        } catch(const std::exception& e) {
            notify(fmt::format("Failed to initialize dongle: {}", e.what()));
        }

        load_config();

        win32app.on_app_window_message = [this](UINT msg, WPARAM wParam, LPARAM lParam) {
            return win32_on_app_window_message(msg, wParam, lParam);
        };

        win32_global_hotkey(true);
    }

    machine::~machine() {
        leave();

        save_config();
    }

    void machine::clear_hid_state() {
        vk_down.clear();

        m_l = false;
        m_r = false;
        m_m = false;
        m_xdiff = 0;
        m_ydiff = 0;
        m_wheel_delta = 0;
    }

    bool machine::is_global_hotkey_down() {

        return
            vk_down.size() == 2 &&
            std::find(vk_down.begin(), vk_down.end(), VK_LCONTROL) != vk_down.end() &&
            std::find(vk_down.begin(), vk_down.end(), VK_LMENU) != vk_down.end();
    }

    void machine::send_clipboard_text() {
        string text = win32::clipboard::get_text();
        if(text.empty()) {
            notify("Clipboard is empty.");
            return;
        }
        send_text(text);
    }

    void machine::send_text(const std::string& text) {
        if(dongle.connected()) {
            try {
                for(char c : text) {
                    dongle.send_kbd_char(c, KeyUpDownDelayMs);

                    std::this_thread::sleep_for(CharTypeDelay);
                }
            } catch(const std::exception& e) {
                notify(fmt::format("Failed to send clipboard text: {}", e.what()));
            }
        }
    }

    std::vector<ble_central> machine::list_centrals() {
        auto centrals = dongle.list_centrals();
        for(auto& central : centrals) {
            // get central name from central address from central_names map
            auto it = central_names.find(central.address);
            if(it != central_names.end()) {
                central.name = (*it).second;
            }

        }
        return centrals;
    }

    void machine::rename_central(const std::string& address, const std::string& new_name) {
        central_names[address] = new_name;
    }

    void machine::win32_run() {
        win32app.run();
    }

    void machine::enter() {
#if _DEBUG
        ::OutputDebugString(L"entering\n");

        while(hid::is_any_key_pressed()) {
            ::OutputDebugStringA("engaged\n");
            // sleep for 100ms to avoid flooding the debug output
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        ::OutputDebugStringA("engaged cleared, entering\n");

#endif

        win32sni.update_icon(IconConnected);
        win32_global_hotkey(false); // no need for hotkey anymore
        win32_ll_hook(true);
        m_initialised = false;
        is_remoting = true;
    }

    void machine::leave() {

#if _DEBUG
        ::OutputDebugString(L"leaving\n");

        while(hid::is_any_key_pressed()) {
            ::OutputDebugStringA("engaged\n");
            // sleep for 100ms to avoid flooding the debug output
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        }

        ::OutputDebugStringA("engaged cleared, leaving\n");
#endif

        win32sni.update_icon(IconDisconnected);
        win32_ll_hook(false);
        win32_global_hotkey(true);  // hotkey can "enter()"
        clear_hid_state();
        dongle.send_flush();
        m_initialised = false;
        is_remoting = false;
    }

    void machine::toggle() {
        if(is_remoting)
            leave();
        else
            enter();
    }

    LRESULT machine::win32_on_app_window_message(UINT msg, WPARAM wParam, LPARAM lParam) {

        if(msg == OWN_WM_NOTIFY_ICON_MESSAGE) {
            switch(lParam) {
                case WM_LBUTTONUP:
                    //toggle();
                    open_config();
                    break;
                case WM_RBUTTONUP:
                    // show context menu
                    win32_build_systray();
                    break;
            }
        } else if(msg == WM_COMMAND) {
            int loword_wparam = LOWORD(wParam);
            string id = win32pm.id_from_loword_wparam(loword_wparam);
            if(id == "x") {
                ::PostQuitMessage(0);
            } else if(id == "clip") {
                send_clipboard_text();
            } else if(id == "cfg") {
                open_config();
            }
        } else if(win32app.is_hotkey_message(msg, wParam, 1)) {
            enter();
        }

        return 0;
    }

    void machine::win32_build_systray(bool show) {

        win32::popup_menu& m = win32pm;

        dongle.refresh();

        // this menu can be rebuilt on the fly when needed (clear, add items)
        m.clear();

        m.add("clip", "Send clipboard text", !dongle.connected());
        m.add("cfg", "Configure");
        m.add("", dongle.connected() ? fmt::format("port: {}", dongle.get_port_name()) : "not connected", true);

        m.separator();
        m.add("x", "&Exit");

        if(show) m.show();
    }

    void machine::win32_ll_hook(bool install) {
        if(install) {
            win32app.install_low_level_keyboard_hook([this](UINT_PTR msg, KBDLLHOOKSTRUCT& kbdll) {
                key_event e{static_cast<uint8_t>(kbdll.vkCode), msg == WM_KEYDOWN};

                if(is_global_hotkey_down()) {
                    leave();
                } else {
                    on_key_event(e);
                }

                return !is_remoting;
            });

            win32app.install_low_level_mouse_hook([this](win32::app::mouse_hook_data mhd) {

                // find out start corordinates
                POINT pt;
                if(::GetCursorPos(&pt)) {
                    m_x_start = pt.x;
                    m_y_start = pt.y;
                    m_x = pt.x;
                    m_y = pt.y;
                    m_xdiff = 0;
                    m_ydiff = 0;
                    m_wheel_delta = 0;
                    m_initialised = true;
                }

                // center mouse cursor
                int scr_w = GetSystemMetrics(SM_CXSCREEN);
                int scr_h = GetSystemMetrics(SM_CYSCREEN);
                ::SetCursorPos(scr_w / 2, scr_h / 2);

                // todo: restore mouse cursor position after remoting ends

                auto e = to_mouse_event(mhd);
                on_mouse_event(e);

                return !is_remoting;
            });

        } else {
            // uninstall all anyway, because setting may change during the session
            win32app.uninstall_low_level_keyboard_hook();
            win32app.uninstall_low_level_mouse_hook();

            ::SetCursorPos(m_x_start, m_y_start);
        }
    }

    void machine::win32_global_hotkey(bool install) {
        if(install) {
            //win32app.register_global_hotkey(1, 0x4B /* K */, MOD_SHIFT | MOD_WIN | MOD_NOREPEAT);
            win32app.register_global_hotkey(1, 0, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT);
        } else {
            win32app.unregister_global_hotkey(1);
        }
    }

    mouse_event machine::to_mouse_event(win32::app::mouse_hook_data mhd) {
        mouse_event e{mhd.pt.x, mhd.pt.y, mouse_button::none, false, mhd.wheel_delta};

        switch(mhd.msg) {
            case WM_LBUTTONDOWN:
                e.button = mouse_button::left;
                e.down = true;
                break;
            case WM_LBUTTONUP:
                e.button = hydra::mouse_button::left;
                e.down = false;
                break;
            case WM_RBUTTONDOWN:
                e.button = hydra::mouse_button::right;
                e.down = true;
                break;
            case WM_RBUTTONUP:
                e.button = hydra::mouse_button::right;
                e.down = false;
                break;
            case WM_MBUTTONDOWN:
                e.button = hydra::mouse_button::middle;
                e.down = true;
                break;
            case WM_MBUTTONUP:
                e.button = hydra::mouse_button::middle;
                e.down = false;
                break;
            case WM_XBUTTONUP:
                e.button = hydra::mouse_button::x;
                e.down = false;
                break;
            case WM_XBUTTONDOWN:
                e.button = hydra::mouse_button::x;
                e.down = true;
                break;
            case WM_MOUSEMOVE:
                // nothing, point is already set
                break;
            case WM_MOUSEHWHEEL:
                // nothing, mouse wheel delta is already set
                break;
            default:
                e.button = hydra::mouse_button::none;
                e.down = false;
                break;
        }

        return e;
    }

    std::string machine::get_config_file_path() {
        return (fs::path{win32::shell::get_local_app_data_path()} / APP_SHORT_NAME / "config.ini").string();
    }

    void machine::load_config() {
        mouse_sensitivity = cfg.get_int_value("mouse_sensitivity", 100);

        central_names.clear();
        for(const string& addr : cfg.list_keys("central_names")) {
            string name = cfg.get_value(addr, "central_names");
            if(!name.empty()) {
                central_names[addr] = name;
            }
        }
    }

    void machine::save_config() {
        cfg.set_value("mouse_sensitivity", mouse_sensitivity);

        for(auto& cn : central_names) {
            cfg.set_value(cn.first, cn.second, "central_names");
        }

        cfg.commit();
    }

    void machine::open_config() {
        hydra::ui::control app{*this};
        app.run();
    }
}