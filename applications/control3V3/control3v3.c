#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <gui/elements.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    int led_state;
} PluginState;

static void render_callback(Canvas* const canvas, void* ctx) {
    const PluginState* plugin_state = acquire_mutex((ValueMutex*)ctx, 25);
    if(plugin_state == NULL) {
        return;
    }
    // border around the edge of the screen
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontPrimary);

    if (plugin_state->led_state == 0) {
        canvas_draw_icon(canvas, 33, 25, &I_Space_65x18);
        elements_multiline_text_aligned(canvas, 45, 36, AlignLeft, AlignBottom, "3v3 OFF");
        canvas_set_color(canvas, ColorBlack);
    }

    else {
        canvas_draw_icon(canvas, 33, 25, &I_Space_65x18);
        elements_multiline_text_aligned(canvas, 45, 36, AlignLeft, AlignBottom, "3v3 ON");
        canvas_set_color(canvas, ColorBlack);
    }

    release_mutex((ValueMutex*)ctx, plugin_state);
}

static void input_callback(InputEvent* input_event, osMessageQueueId_t event_queue) {
    furi_assert(event_queue);

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    osMessageQueuePut(event_queue, &event, 0, osWaitForever);
}

static void control3v3_state_init(PluginState* const plugin_state) {
    plugin_state->led_state = 0;
}

int32_t control3v3_app(void* p) {
    osMessageQueueId_t event_queue = osMessageQueueNew(8, sizeof(PluginEvent), NULL);

    PluginState* plugin_state = malloc(sizeof(PluginState));
    control3v3_state_init(plugin_state);
    ValueMutex state_mutex;
    if (!init_mutex(&state_mutex, plugin_state, sizeof(PluginState))) {
        FURI_LOG_E("Control3V3", "cannot create mutex\r\n");
        free(plugin_state);
        return 255;
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, &state_mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);


    PluginEvent event;
    for(bool processing = true; processing;) {
        osStatus_t event_status = osMessageQueueGet(event_queue, &event, NULL, 100);
        PluginState* plugin_state = (PluginState*)acquire_mutex_block(&state_mutex);

        if(event_status == osOK) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyRight:
                    case InputKeyLeft:
                    case InputKeyUp:
                    case InputKeyDown:
                    case InputKeyOk:
                        if (plugin_state->led_state == 1) {
                            furi_hal_power_disable_external_3_3v();
                            plugin_state->led_state = 0;
                        }
                        else {
                            furi_hal_power_enable_external_3_3v();
                            plugin_state->led_state = 1;
                        }
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    }
                }
            }
        } else {
            FURI_LOG_D("Control3V3", "osMessageQueue: event timeout");
            // event timeout
        }

        view_port_update(view_port);
        release_mutex(&state_mutex, plugin_state);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    osMessageQueueDelete(event_queue);

    return 0;
}