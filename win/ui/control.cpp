#include "control.h"

namespace hydra::ui {

    namespace w = grey::widgets;
    using namespace std;

    control::control(hydra::machine& machine) :
        wnd_main{"Control", &is_running},
        wnd_emu{"Emulator", &wnd_emu_visible},
        machine{machine} {
        app = grey::app::make(APP_LONG_NAME, 500, 250);
        app->win32_can_resize = false;
        //app->initial_theme_id = "duck_red";

        wnd_main
            .fill_viewport()
            .no_titlebar()
            .no_resize()
            .no_scroll()
            .border(0);

        wnd_emu
            .border(1)
            .size(400, 300);

        centrals_container
            .border()
            .resize(0, 100 * w::scale);

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
            render_centrals();

            if(wnd_emu_visible) {
                with_window(wnd_emu, {
                    render_emulator();
                });
            }

            render_status_bar();
        });
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
        if(w::button(ICON_MD_TOUCH_APP)) {
            wnd_emu_visible = !wnd_emu_visible;
        }
        w::tooltip("Toggle emulator window");

        if(!error_message.empty()) {
            w::sl();
            w::label(ICON_MD_ERROR, w::emphasis::error);
            w::tooltip(error_message);
        }

    }

    void control::render_centrals() {

        with_container(centrals_container,
            for(ble_central& c : centrals) {

                if(!c.is_default) {
                    if(w::button(string{ICON_MD_POWER} + "##" + c.address)) {
                        machine.dongle.activate_central(c.address);
                        centrals_dirty = true;
                    }
                } else {
                    w::label("");
                }

                w::sl(40 * w::scale);
                w::label(ICON_MD_FIBER_MANUAL_RECORD, c.is_default ? w::emphasis::primary : w::emphasis::none);
                w::sl();
                if(w::input(c.name, "##" + c.address, true, 200 * w::scale)) {
                    machine.rename_central(c.address, c.name);
                    machine.save_config();
                }

                w::sl();
                w::label(c.address, 0, false);
            }
        );

        w::slider(machine.mouse_sensitivity, 0, 300, "mouse sensitivity");
        w::sl();
        if(w::button(ICON_MD_RESTART_ALT)) {
            machine.mouse_sensitivity = 100;
        }
    }

    void control::render_status_bar() {
        with_status_bar(
            w::label(ICON_MD_CABLE, w::emphasis::primary);
            w::sl();
            w::label(machine.dongle.get_port_name());

            w::sl();
            w::label("|", 0, false);

            w::sl();
            w::label("todo");
        );
    }

    void control::render_emulator() {

        w::tab_bar tb{"emu"};

        {
            auto tab = tb.next_tab("keyboard");
            if(tab) {
                if(w::button(ICON_MD_KEYBOARD " type", w::emphasis::primary, emu_text.size() > 0)) {
                    machine.send_text(emu_text);
                }
                w::sl();
                if(w::button(ICON_MD_DELETE " clear", w::emphasis::error, emu_text.size() > 0)) {
                    emu_text.clear();
                }

                w::input_ml("##emu_text", emu_text, 0.0f, false, true);
            }
        }

        {
            auto tab = tb.next_tab("mouse");
            if(tab) {
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
        }

    }
}
