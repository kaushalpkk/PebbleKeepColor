#include "pebble.h"
#include "pebble_fonts.h"

#include "KeepNote.h"

Window* listWindow;
MenuLayer* menuLayer;
TextLayer* loadingLayer;

char items[50][20];
uint8_t numOfItems = 0;

bool loading = true;
int8_t pickedItem = -1;

bool displayingNote = false;

void sendSelection()
{
    DictionaryIterator *iterator;
    app_message_outbox_begin(&iterator);

    dict_write_uint8(iterator, 0, 2);
    dict_write_uint8(iterator, 1, pickedItem);

    app_message_outbox_send();

    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
    app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

void list_part_received(DictionaryIterator *received)
{
    uint8_t index = dict_find(received, 1)->value->uint8;
    numOfItems = dict_find(received, 2)->value->uint8;

    bool listFinished = false;

    for (uint8_t i = 0; i < 3; i++) {
        uint8_t listPos = index + i;
        if (listPos > numOfItems - 1) {
            listFinished = true;
            break;
        }

        strcpy(items[listPos], dict_find(received, i + 3)->value->cstring);
    }

    if (index + 3 == numOfItems)
        listFinished = true;

    if (listFinished) {
        loading = false;
    }

    else {
        if (pickedItem != -1) {
            sendSelection();
            return;
        }

        DictionaryIterator *iterator;
        app_message_outbox_begin(&iterator);
        dict_write_uint8(iterator, 0, 1);
        dict_write_uint8(iterator, 1, index + 3);
        app_message_outbox_send();

        app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
        app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
    }

    layer_set_hidden((Layer*) loadingLayer, true);
    layer_set_hidden((Layer*) menuLayer, false);

    menu_layer_reload_data(menuLayer);
}

void received_data(DictionaryIterator *received, void *context)
{
    if (displayingNote) {
        note_data_received(received);
        return;
    }

    uint8_t id = dict_find(received, 0)->value->uint8;

    switch (id) {
        case 0:
            list_part_received(received);
            break;
        case 1:
            displayingNote = true;
            note_init();
            note_data_received(received);
            break;
    }
}

// A callback is used to specify the amount of sections of menu items
// With this, you can dynamically add and remove sections
uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data)
{
    return 1;
}

// Each section has a number of items;  we use a callback to specify this
// You can also dynamically add and remove items using this
uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data)
{
    return numOfItems;
}


int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data)
{
    return 27;
}



// This is the menu item draw callback where you specify what each item should look like
void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, items[cell_index->row], fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                       GRect(3, 3, 141, 28), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}


// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data)
{
    pickedItem = cell_index->row;

    if (!loading)
        sendSelection();
}


void window_load(Window *me)
{
    layer_set_hidden((Layer*) menuLayer, true);
    layer_set_hidden((Layer*) loadingLayer, false);
    loading = true;
    pickedItem = -1;
    displayingNote = false;

    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

    DictionaryIterator *iterator;
    app_message_outbox_begin(&iterator);

    dict_write_uint8(iterator, 0, 0);

    app_message_outbox_send();
}


int main()
{
    app_message_register_inbox_received(received_data);
    app_message_open(120, 20);

//  GRect windowBounds = GRect(0, 0, 0, 0);
    GRect windowBounds = GRect(0, 0, 144, 168 - 28);

    listWindow = window_create();

    Layer* topLayer = window_get_root_layer(listWindow);
    window_set_background_color(listWindow, GColorChromeYellow);
    loadingLayer = text_layer_create(windowBounds);
    text_layer_set_font(loadingLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(loadingLayer, "Loading...");

    layer_add_child(topLayer, (Layer*) loadingLayer);

    menuLayer = menu_layer_create(windowBounds);

    // Set all the callbacks for the menu layer
    menu_layer_set_callbacks(menuLayer, NULL, (MenuLayerCallbacks) {
        .get_num_sections = menu_get_num_sections_callback,
         .get_num_rows = menu_get_num_rows_callback,
          .get_cell_height = menu_get_row_height_callback,
           .draw_row = menu_draw_row_callback,
            .select_click = menu_select_callback,
    });

    layer_add_child(topLayer, (Layer*) menuLayer);


// Bind the menu layer's click config provider to the window for interactivity
    menu_layer_set_click_config_onto_window(menuLayer, listWindow);

    window_set_window_handlers(listWindow, (WindowHandlers) {
        .appear = window_load,
    });

    window_stack_push(listWindow, true /* Animated */);

    app_event_loop();
    window_stack_pop_all(false);

    return 0;
}
