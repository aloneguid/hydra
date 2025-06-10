#include "control.h"

namespace hydra::ui {

    namespace w = grey::widgets;
    using namespace std;

    control::control(hydra::machine& machine) :
        wnd_main{"Control", &is_running},
        machine{machine} {
        app = grey::app::make(APP_LONG_NAME, 500, 600);

        wnd_main
            .fill_viewport()
            .no_titlebar()
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
            w::slider(machine.mouse_sensitivity, 0.1f, 3.f, "mouse sensitivity");
            render_centrals();
            render_emulator();
        });
    }

    void control::render_toolbar() {
        w::label(ICON_MD_CABLE, w::emphasis::primary); w::sl(); w::label(machine.dongle.get_port_name());

        w::sl();

        if(w::button(ICON_MD_REFRESH, w::emphasis::primary)) {
            centrals_dirty = true;
        }
        w::tooltip("Refresh centrals");

        if(!error_message.empty()) {
            w::label(ICON_MD_ERROR, w::emphasis::error);
            w::tooltip(error_message);
        }
    }

    void control::render_centrals() {

        w::sep("centrals");
        for(ble_central& c : centrals) {
            if(w::button(string{ICON_MD_POWER} + "##" + c.address, c.is_default ? w::emphasis::primary : w::emphasis::none, !c.is_default)) {
                machine.dongle.activate_central(c.address);
                centrals_dirty = true;
            }
            w::sl();
            if(w::input(c.name, "##" + c.address, true, 200 * w::scale)) {
                machine.rename_central(c.address, c.name);
                machine.save_config();
            }

            w::sl();
            w::label(c.address, 0, false);
        }
    }

    void control::render_emulator() {
        w::sep("controls");

        w::input_ml("##emu_text", emu_text, 10U, false, true);
        if(w::button("type text remotely")) {
            machine.send_text(emu_text);
        }

        // mouse
        w::label("mouse:");
        w::slider(emu_mouse_speed, 0.1f, 50.0f, "speed");

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
