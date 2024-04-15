// (c)2024, Arthur van Hoff, Artfahrt Inc.
//

#include "button.h"

void Button::idle(unsigned long now) 
{
    int clicks = 0;
    bool held = false;

    noInterrupts();
    if (released_tm != 0) {
        if (now - released_tm > 300) {
            clicks = click_count;
            released_tm = 0;
            pressed_tm = 0;
            click_count = 0;
        }
    } else if (pressed_tm != 0) {
        if (now - pressed_tm > 3000) {
            pressed_tm = 0;
            click_count = 0;
            held = true;
        }
    }
    interrupts();

    if (clicks) {
        //dprintf("button %s: clicks=%d", name, clicks);
        pressed(clicks);
    } else if (held) {
        //dprintf("button %s: hold", name);
        hold();
    }
}

Button *Button::button = NULL;

void Button::handle_interrupt()
{
    if (button != NULL) {
        bool state = button->down();
        if (state != button->state) {
            button->state = state;
            unsigned long now = millis();
            if (state) {
                if (button->released_tm != 0) {
                    if (now - button->released_tm < 10) {
                        // debounce
                        button->released_tm = 0;
                        return;
                    }
                }
                button->click_count += 1;
                button->pressed_tm = now;
                button->released_tm = 0;
            } else if (button->pressed_tm != 0) {
                if (now - button->pressed_tm > 250) {
                    button->click_count = 1;
                }
                button->released_tm = now;
            }
        }
    }
}