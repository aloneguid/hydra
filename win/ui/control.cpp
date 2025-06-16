#include "control.h"
#include <thread>
#include <numbers>
#include <fmt/format.h>
#include "../hid.h"

namespace hydra::ui {

    namespace w = grey::widgets;
    using namespace std;

    control::control(hydra::machine& machine) :
        wnd_main{"Control", &is_running},
        machine{machine} {
        app = grey::app::make(APP_LONG_NAME, 500, 350);
        app->win32_can_resize = false;
        //app->initial_theme_id = "duck_red";

        wnd_main
            .fill_viewport()
            .no_titlebar()
            .no_resize()
            .no_scroll()
            .border(0);

        centrals = machine.list_centrals();
    }

    control::~control() {
        machine.save_config();
    }

    void control::run() {
        app->run([this](const grey::app& app) {
            run_frame();

            return is_running;
        });
    }

    void control::run_frame() {
        if(centrals_dirty) {

            try {
                centrals = machine.list_centrals();
                error_message.clear();
            } catch(const std::exception& e) {
                error_message = e.what();
            }
            centrals_dirty = false;
        }

        with_window(wnd_main, {
            render_toolbar();

            render_main_tabs();

            render_status_bar();
        });
    }

    void control::render_main_tabs() {

        w::tab_bar tb{"tbmain"};

        {
            auto tab = tb.next_tab("devices");
            if(tab) {
                render_centrals();
            }
        }

        {
            auto tab = tb.next_tab("typer");
            if(tab) {
                render_typer();
            }
        }

        {
            auto tab = tb.next_tab("mouse");
            if(tab) {
                render_mouse_emulator();
            }
        }

        {
            auto tab = tb.next_tab("settings");
            if(tab) {
                render_settings();
            }
        }
    }

    void control::render_toolbar() {

        if(w::button(ICON_MD_REFRESH)) {
            centrals_dirty = true;
        }
        w::tooltip("Refresh centrals");

        w::sl();
        if(w::button(ICON_MD_LOCK)) {
            machine.enter();
        }
        w::tooltip("Enter capture mode. To exit, press ctrl+alt.");

        w::sl();
        if(w::button(ICON_MD_DASHBOARD)) {
            machine.dongle.get_dashboard();
        }

        w::sl();
        if(w::button(ICON_MD_DASHBOARD_CUSTOMIZE)) {
            machine.dongle.reset_dashboard();
        }

        if(!error_message.empty()) {
            w::sl();
            w::label(ICON_MD_ERROR, w::emphasis::error);
            w::tooltip(error_message);
        }

    }

    void control::render_centrals() {

        with_container(centrals_container,
            for(ble_central& c : centrals) {

                w::label(ICON_MD_FIBER_MANUAL_RECORD, c.is_default ? w::emphasis::primary : w::emphasis::none);
                if(!c.is_default) {
                    if(w::is_hovered()) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                    if(w::is_leftclicked()) {
                        machine.dongle.activate_central(c.address);
                        centrals_dirty = true;
                    }
                }

                w::sl();
                if(w::input(c.name, "##" + c.address, true, 200 * w::scale)) {
                    machine.rename_central(c.address, c.name);
                    machine.save_config();
                }

                w::sl();
                w::label(c.address, 0, false);
            }
        );
    }

    void control::render_typer() {
        if(w::button(ICON_MD_KEYBOARD " type", w::emphasis::primary, emu_text.size() > 0)) {
            machine.send_text(emu_text);
        }
        w::sl();
        if(w::button(ICON_MD_DELETE " clear", w::emphasis::error, emu_text.size() > 0)) {
            emu_text.clear();
        }

        for(uint8_t vk : machine.get_vk_down()) {
            uint8_t sc = hid::vk_to_scancode(vk);
            uint8_t hid1 = hid::vk_to_hid(vk);

            w::sl();
            // print hex value of the vk
            w::label(fmt::format("VK: 0x{:02X} SCAN: 0x{:02X}, HID1: 0x{:02X}", vk, sc, hid1));
        }

        w::input_ml("##emu_text", emu_text, 0.0f, false, true);
    }

    void control::render_status_bar() {
        with_status_bar(
            w::label(ICON_MD_CABLE, w::emphasis::primary);
            w::sl();
            w::label(machine.dongle.get_port_name());

            w::sl();
            w::label("|", 0, false);

            for(auto& c : centrals) {
                if(c.is_default) {
                    w::sl();
                    w::label(c.name);
                    break;
                }
            }

        );
    }

    void control::render_mouse_emulator() {

        if(w::button(ICON_MD_CIRCLE)) {
            // current coordinate is top of the circle. Move mouse in a circular fashion and come back to the starting position. We can only perform relative moves.
            // The circle radius is 100 pixels.
            constexpr float radius = 10.0f;
            constexpr float steps = 100.0f;
            constexpr float angle_step = 2 * std::numbers::pi_v<float> / steps;
            for(float i = 0; i < steps; ++i) {
                float angle = i * angle_step;
                int x = static_cast<int>(radius * std::cos(angle));
                int y = static_cast<int>(radius * std::sin(angle));
                machine.dongle.send_mouse(false, false, false, x, y, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        w::sl();
        if(w::button(ICON_MD_ARROW_DROP_DOWN)) {
            // move down pixel by pixel
            for(int i = 0; i < 1000; i++) {
                machine.dongle.send_mouse(false, false, false, 1, 1, 0);
            }
        }

        w::slider(emu_mouse_speed, 1, 100, "speed in pixels");

        w::label(" ");
        w::sl(35 * w::scale);
        if(w::button(ICON_MD_NORTH)) {
            machine.dongle.send_mouse(false, false, false,
                0, -emu_mouse_speed, 0);
        }

        w::sl(100 * w::scale);
        if(w::button("L")) {
            machine.dongle.send_mouse(true, false, false, 0, 0, 0);
        }
        w::sl();
        if(w::button("M")) {
            machine.dongle.send_mouse(false, true, false, 0, 0, 0);
        }
        w::sl();
        if(w::button("R")) {
            machine.dongle.send_mouse(false, false, true, 0, 0, 0);
        }

        if(w::button(ICON_MD_WEST)) {
            machine.dongle.send_mouse(false, false, false,
                -emu_mouse_speed, 0, 0);
        }
        w::sl(60 * w::scale);
        if(w::button(ICON_MD_EAST)) {
            machine.dongle.send_mouse(false, false, false,
                emu_mouse_speed, 0, 0);
        }
        w::label(" ");
        w::sl(35 * w::scale);
        if(w::button(ICON_MD_SOUTH)) {
            machine.dongle.send_mouse(false, false, false, 0, emu_mouse_speed, 0);
        }
    }

    void control::render_settings() {
        if(w::button(ICON_MD_RESTART_ALT)) {
            machine.mouse_sensitivity = 100;
        }
        w::sl();
        w::slider(machine.mouse_sensitivity, 0, 300, "mouse sensitivity");

        if(w::button(ICON_MD_RESTART_ALT)) {
            machine.key_press_delay_ms = 100;
        }
        w::sl();
        w::slider(machine.key_press_delay_ms, 0, 1000, "key press delay (ms)");
    }
}
