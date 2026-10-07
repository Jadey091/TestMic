#include "linux_internal.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <string.h>

typedef struct LinuxMenuRow {
    const char *label;
    BongoCatMenuAction action;
} LinuxMenuRow;

#define LINUX_MENU_HINT ((BongoCatMenuAction)-4)
#define LINUX_MENU_MOTIONS ((BongoCatMenuAction)-5)
#define LINUX_MENU_EXPRESSIONS ((BongoCatMenuAction)-6)

typedef struct LinuxMenuPalette {
    unsigned long surface, field, border, text, muted, accent;
} LinuxMenuPalette;

static BongoCatMenuAction finish(const BongoCatMenuLabels *labels,
    BongoCatMenuAction action) {
    if (labels->restore) labels->restore(labels->preview_userdata, action);
    return action;
}

static unsigned long color(Display *display, int screen, const char *value) {
    XColor resolved;
    Colormap map = DefaultColormap(display, screen);
    return XParseColor(display, map, value, &resolved) &&
        XAllocColor(display, map, &resolved) ? resolved.pixel :
        WhitePixel(display, screen);
}

static LinuxMenuPalette palette(Display *display, bool dark) {
    int screen = DefaultScreen(display);
    LinuxMenuPalette value;
    value.surface = color(display, screen, dark ? "#21242b" : "#ffffff");
    value.field = color(display, screen, dark ? "#2a2e37" : "#f3f5f8");
    value.border = color(display, screen, dark ? "#3b424f" : "#d8dee8");
    value.text = color(display, screen, dark ? "#f4f7fb" : "#182230");
    value.muted = color(display, screen, dark ? "#9aa4b2" : "#667085");
    value.accent = color(display, screen, "#54aeff");
    return value;
}

static void draw_menu(Display *display, Window window, GC gc,
    const LinuxMenuRow *rows, int count, int hover,
    const LinuxMenuPalette *colors) {
    XSetForeground(display, gc, colors->surface); XFillRectangle(display, window, gc,
        0, 0, 340, (unsigned)(count * 34 + 16));
    XSetForeground(display, gc, colors->border); XDrawRectangle(display, window, gc,
        0, 0, 339, (unsigned)(count * 34 + 15));
    for (int index = 0; index < count; ++index) {
        int y = 8 + index * 34;
        bool hint = rows[index].action == LINUX_MENU_HINT;
        if (index == hover && !hint) {
            XSetForeground(display, gc, colors->field);
            XFillRectangle(display, window, gc, 8, y, 324, 32);
        }
        XSetForeground(display, gc, hint ? colors->muted :
            index == hover ? colors->accent : colors->text);
        XDrawString(display, window, gc, 20, y + 21, rows[index].label,
            (int)strlen(rows[index].label));
    }
    XFlush(display);
}

static int popup_rows(Display *display, Window owner, const LinuxMenuRow *rows,
    int count, const BongoCatMenuLabels *labels) {
    if (!display || !rows || count < 1) return -1;
    Window root; int root_x, root_y, win_x, win_y; unsigned mask;
    XQueryPointer(display, DefaultRootWindow(display), &root, &owner,
        &root_x, &root_y, &win_x, &win_y, &mask);
    int height = count * 34 + 16;
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));
    if (root_x > screen_width - 342) root_x = screen_width - 342;
    if (root_y > screen_height - height - 2) root_y = screen_height - height - 2;
    LinuxMenuPalette colors = palette(display, labels->dark_theme);
    XSetWindowAttributes attributes = {0};
    attributes.override_redirect = True;
    attributes.background_pixel = colors.surface;
    attributes.border_pixel = colors.border;
    Window menu = XCreateWindow(display, DefaultRootWindow(display), root_x,
        root_y, 340, (unsigned)height, 1, CopyFromParent, InputOutput,
        CopyFromParent, CWOverrideRedirect | CWBackPixel | CWBorderPixel,
        &attributes);
    if (!menu) return -1;
    XSelectInput(display, menu, ExposureMask | ButtonPressMask |
        PointerMotionMask | LeaveWindowMask | KeyPressMask);
    XMapRaised(display, menu);
    int pointer_grab = XGrabPointer(display, menu, False,
        ButtonPressMask | PointerMotionMask, GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime);
    int keyboard_grab = XGrabKeyboard(display, menu, False,
        GrabModeAsync, GrabModeAsync, CurrentTime);
    GC gc = XCreateGC(display, menu, 0, NULL);
    if (pointer_grab != GrabSuccess || keyboard_grab != GrabSuccess || !gc) {
        if (pointer_grab == GrabSuccess) XUngrabPointer(display, CurrentTime);
        if (keyboard_grab == GrabSuccess) XUngrabKeyboard(display, CurrentTime);
        if (gc) XFreeGC(display, gc);
        XDestroyWindow(display, menu); XFlush(display);
        return -1;
    }
    int selected = -1, hover = -1;
    while (selected < 0) {
        if (!XPending(display) && labels->preview_tick) {
            labels->preview_tick(labels->preview_userdata);
            SDL_Delay(16);
            continue;
        }
        XEvent event; XNextEvent(display, &event);
        if (event.type == Expose) draw_menu(display, menu, gc, rows, count,
            hover, &colors);
        else if (event.type == MotionNotify) {
            int next = event.xmotion.y >= 8 ? (event.xmotion.y - 8) / 34 : -1;
            next = next >= 0 && next < count &&
                rows[next].action != LINUX_MENU_HINT ? next : -1;
            if (next != hover && labels->preview)
                labels->preview(labels->preview_userdata, next >= 0 ?
                    rows[next].action : BONGO_CAT_MENU_NONE);
            hover = next;
            draw_menu(display, menu, gc, rows, count, hover, &colors);
        } else if (event.type == LeaveNotify) {
            hover = -1;
            if (labels->preview) labels->preview(labels->preview_userdata,
                BONGO_CAT_MENU_NONE);
            draw_menu(display, menu, gc, rows, count, hover, &colors);
        } else if (event.type == ButtonPress) {
            int next = event.xbutton.y >= 8 ? (event.xbutton.y - 8) / 34 : -1;
            if (next < 0 || next >= count) selected = -2;
            else if (rows[next].action != LINUX_MENU_HINT) selected = next;
        } else if (event.type == KeyPress &&
            XLookupKeysym(&event.xkey, 0) == XK_Escape) selected = -2;
    }
    XUngrabPointer(display, CurrentTime); XUngrabKeyboard(display, CurrentTime);
    XFreeGC(display, gc);
    XDestroyWindow(display, menu); XFlush(display);
    return selected >= 0 ? rows[selected].action : BONGO_CAT_MENU_NONE;
}

BongoCatMenuAction bongo_cat_linux_context_menu(BongoCatPlatform *platform,
    const BongoCatMenuLabels *labels) {
    if (!platform || !labels) return BONGO_CAT_MENU_NONE;
    SDL_PropertiesID properties = SDL_GetWindowProperties(platform->window);
    Display *display = SDL_GetPointerProperty(properties,
        SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
    Window owner = (Window)SDL_GetNumberProperty(properties,
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    if (!display || !owner) return finish(labels, BONGO_CAT_MENU_NONE);
    char pass[BONGO_CAT_ID_CAP], top[BONGO_CAT_ID_CAP];
    snprintf(pass, sizeof(pass), "%s%s", labels->pass_through_checked ? "[x] " : "",
        labels->pass_through);
    snprintf(top, sizeof(top), "%s%s", labels->always_on_top_checked ? "[x] " : "",
        labels->always_on_top);
    LinuxMenuRow main_rows[10]; int main_count = 0;
    main_rows[main_count++] = (LinuxMenuRow){labels->preferences, BONGO_CAT_MENU_PREFERENCES};
    main_rows[main_count++] = (LinuxMenuRow){labels->hide, BONGO_CAT_MENU_HIDE};
    main_rows[main_count++] = (LinuxMenuRow){pass, BONGO_CAT_MENU_PASS_THROUGH};
    main_rows[main_count++] = (LinuxMenuRow){top, BONGO_CAT_MENU_ALWAYS_ON_TOP};
    main_rows[main_count++] = (LinuxMenuRow){labels->window_size, -1};
    main_rows[main_count++] = (LinuxMenuRow){labels->opacity, -2};
    if (labels->motion_count) main_rows[main_count++] =
        (LinuxMenuRow){labels->motion, LINUX_MENU_MOTIONS};
    if (labels->expression_count) main_rows[main_count++] =
        (LinuxMenuRow){labels->expression, LINUX_MENU_EXPRESSIONS};
    main_rows[main_count++] = (LinuxMenuRow){labels->model, -3};
    main_rows[main_count++] = (LinuxMenuRow){labels->exit, BONGO_CAT_MENU_EXIT};
    BongoCatMenuAction action = popup_rows(display, owner, main_rows,
        main_count, labels);
    if (action == (BongoCatMenuAction)-1) {
        const int values[] = {50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200};
        LinuxMenuRow rows[17]; char text[16][16];
        for (int i = 0; i < 16; ++i) { snprintf(text[i], sizeof(text[i]), "%d%%", values[i]);
            rows[i] = (LinuxMenuRow){text[i], BONGO_CAT_MENU_SCALE_50 + i}; }
        rows[16] = (LinuxMenuRow){labels->wheel_size_hint, LINUX_MENU_HINT};
        action = popup_rows(display, owner, rows, 17, labels);
    } else if (action == (BongoCatMenuAction)-2) {
        const int values[] = {10,20,30,40,50,60,70,80,90,100};
        LinuxMenuRow rows[11]; char text[10][16];
        for (int i = 0; i < 10; ++i) { snprintf(text[i], sizeof(text[i]), "%d%%", values[i]);
            rows[i] = (LinuxMenuRow){text[i], BONGO_CAT_MENU_OPACITY_10 + i}; }
        rows[10] = (LinuxMenuRow){labels->wheel_opacity_hint, LINUX_MENU_HINT};
        action = popup_rows(display, owner, rows, 11, labels);
    } else if (action == (BongoCatMenuAction)-3) {
        LinuxMenuRow rows[BONGO_CAT_MODEL_CAP + 1];
        for (size_t i = 0; i < labels->model_count; ++i)
            rows[i] = (LinuxMenuRow){labels->model_names[i],
                BONGO_CAT_MENU_MODEL_FIRST + (int)i};
        rows[labels->model_count] = (LinuxMenuRow){labels->add_model,
            BONGO_CAT_MENU_MODEL_ADD};
        action = popup_rows(display, owner, rows,
            (int)labels->model_count + 1, labels);
    } else if (action == LINUX_MENU_MOTIONS) {
        LinuxMenuRow rows[BONGO_CAT_BEHAVIOR_CAP];
        for (size_t i = 0; i < labels->motion_count; ++i)
            rows[i] = (LinuxMenuRow){labels->motion_names[i],
                BONGO_CAT_MENU_MOTION_FIRST + (int)i};
        action = popup_rows(display, owner, rows, (int)labels->motion_count, labels);
    } else if (action == LINUX_MENU_EXPRESSIONS) {
        LinuxMenuRow rows[BONGO_CAT_BEHAVIOR_CAP];
        char text[BONGO_CAT_BEHAVIOR_CAP][BONGO_CAT_ID_CAP];
        for (size_t i = 0; i < labels->expression_count; ++i) {
            snprintf(text[i], sizeof(text[i]), "%s%s",
                i == labels->current_expression ? "[x] " : "",
                labels->expression_names[i]);
            rows[i] = (LinuxMenuRow){text[i], BONGO_CAT_MENU_EXPRESSION_FIRST + (int)i};
        }
        action = popup_rows(display, owner, rows, (int)labels->expression_count, labels);
    }
    return finish(labels, action);
}
#endif
