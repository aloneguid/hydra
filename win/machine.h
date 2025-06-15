#pragma once
#include <vector>
#include "model.h"
#include "win32/app.h"
#include "win32/shell_notify_icon.h"
#include "win32/popup_menu.h"
#include "config/config.h"
#include <chrono>
#include "dongle.h"

namespace hydra {
    class machine {
    public:
        machine();
        ~machine();

        dongle dongle;

        // settings
        int mouse_sensitivity{100};   // mouse sensitivity multiplier
        int key_press_delay_ms{100};
        //ble_central* active_central{nullptr}; // currently active central, if any

        /**
         * @brief Checks if the global hotkey is down when we are in the remote session. This needs to be checked by checking if the corresponding HID report is down.
         * @return 
         */
        bool is_global_hotkey_down();

        // utility
        void send_text(const std::string& text, int key_delay_ms = -1);
        void send_clipboard_text(int key_delay_ms = -1);

        // config
        std::vector<ble_central> list_centrals();
        void rename_central(const std::string& address, const std::string& new_name);

        // win32
        void win32_run();

        void enter();
        void leave();
        void toggle();

        void load_config();
        void save_config();
        void open_config();

    private:
        win32::app win32app;
        win32::shell_notify_icon win32sni;
        win32::popup_menu win32pm;

        bool is_remoting{false};    // machine is in remoting state

        // keyboard state
        std::vector<uint8_t> vk_down;    // vk codes that are pressed down

        // mouse state
        const long long MouseMoveDelayMs{1000 / 60}; // 60 FPS
        std::chrono::steady_clock::time_point m_last_report_time; // last mouse event time
        // mouse
        int m_x_start{0};   // mouse position when capture started
        int m_y_start{0};
        bool m_l{false};    // left button
        bool m_r{false};    // right button
        bool m_m{false};    // middle button
        int m_x{0};
        int m_y{0};
        // mouse movement
        int m_xdiff{0};
        int m_xdiff_acc{0};
        int m_ydiff{0};
        int m_ydiff_acc{0};
        // mouse wheel
        int m_wheel_delta{0}; // positive - up, negative - down


        void notify(const std::string& text);

        // signal handlers
        void on_key_event(const key_event& e);
        void on_mouse_event(const mouse_event& e);

        // win32
        LRESULT win32_on_app_window_message(UINT msg, WPARAM wParam, LPARAM lParam);
        void win32_build_systray(bool show = true);
        void win32_ll_hook(bool install); // true - install, false - uninstall
        void win32_global_hotkey(bool install); // true - install, false - uninstall
        static mouse_event to_mouse_event(win32::app::mouse_hook_data mhd);

        /**
         * @brief Makes sure target HID state is cleared, to avoid any ghosting. This clears all keyboard and mouse state in the machine, plus submits the clear HID report to the target.
         */
        void clear_hid_state();

        // config
        ::common::config cfg;
        static std::string get_config_file_path();
        std::map<std::string, std::string> central_names;

#if _DEBUG
        /**
         * @brief Prints prefix and hex-encoded data to debug terminal
         * @param prefix 
         * @param data 
         */
        void dbg_print(const std::string& prefix, const std::vector<uint8_t>& data);
#endif
    };
}