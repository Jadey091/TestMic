#include "bongo_cat/platform.h"
#include "macos_internal.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

static int instance_lock = -1;
static BongoCatPlatform *active_platform;
static NSObject *instance_observer;
static bool instance_show_pending;

/* SDL 3.2's pinned Cocoa tray layout; kept local to the platform adapter. */
typedef struct BongoCatSDLTrayMenu { NSMenu *menu; } BongoCatSDLTrayMenu;
typedef struct BongoCatSDLTray {
    NSStatusBar *status_bar; NSStatusItem *status_item; BongoCatSDLTrayMenu *menu;
} BongoCatSDLTray;

@interface BongoCatTrayTarget : NSObject {
    NSStatusItem *item_; NSMenu *menu_; BongoCatTrayClick callback_;
    BongoCatModalTick modal_tick_; void *userdata_;
}
- (id)initWithItem:(NSStatusItem *)item menu:(NSMenu *)menu
    callback:(BongoCatTrayClick)callback modalTick:(BongoCatModalTick)modalTick
    userdata:(void *)userdata;
- (void)clicked:(id)sender;
- (void)tickModal:(NSTimer *)timer;
- (void)unbind;
@end

@implementation BongoCatTrayTarget
- (id)initWithItem:(NSStatusItem *)item menu:(NSMenu *)menu
    callback:(BongoCatTrayClick)callback modalTick:(BongoCatModalTick)modalTick
    userdata:(void *)userdata {
    self = [super init];
    if (self) { item_ = item; menu_ = menu; callback_ = callback;
        modal_tick_ = modalTick; userdata_ = userdata; }
    return self;
}
- (void)clicked:(id)sender {
    (void)sender;
    NSEvent *event = [NSApp currentEvent];
    if ([event type] == NSEventTypeLeftMouseUp) callback_(userdata_);
    else {
        if (modal_tick_) modal_tick_(userdata_);
        NSTimer *timer = modal_tick_ ? [NSTimer timerWithTimeInterval:1.0 / 60.0
            target:self selector:@selector(tickModal:) userInfo:nil repeats:YES] : nil;
        if (timer) [[NSRunLoop currentRunLoop] addTimer:timer
            forMode:NSRunLoopCommonModes];
        [NSMenu popUpContextMenu:menu_ withEvent:event forView:[item_ button]];
        [timer invalidate];
    }
}
- (void)tickModal:(NSTimer *)timer {
    (void)timer;
    if (modal_tick_) modal_tick_(userdata_);
}
- (void)unbind {
    [[item_ button] setTarget:nil]; [[item_ button] setAction:nil];
    [item_ setMenu:menu_];
}
@end

static BongoCatTrayTarget *tray_target;

@interface BongoCatModalTarget : NSObject { BongoCatModalTick callback_; void *userdata_; }
- (id)initWithCallback:(BongoCatModalTick)callback userdata:(void *)userdata;
- (void)tick:(NSTimer *)timer;
@end
@implementation BongoCatModalTarget
- (id)initWithCallback:(BongoCatModalTick)callback userdata:(void *)userdata {
    self = [super init];
    if (self) { callback_ = callback; userdata_ = userdata; }
    return self;
}
- (void)tick:(NSTimer *)timer { (void)timer; callback_(userdata_); }
@end

static NSWindow *native_window(BongoCatPlatform *platform) {
    return (__bridge NSWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(platform->window),
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
}

static void configure_capture_window(NSWindow *window) {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [window setSharingType:NSWindowSharingReadOnly];
}

static void show_instance(void) {
    if (!active_platform) { instance_show_pending = true; return; }
    NSWindow *window = native_window(active_platform); configure_capture_window(window);
    [window orderFrontRegardless];
    [NSApp activateIgnoringOtherApps:YES];
    instance_show_pending = false;
}

@interface BongoCatInstanceObserver : NSObject
- (void)showWindow:(NSNotification *)notification;
@end
@implementation BongoCatInstanceObserver
- (void)showWindow:(NSNotification *)notification {
    if (![NSThread isMainThread]) {
        [self performSelectorOnMainThread:@selector(showWindow:)
            withObject:notification waitUntilDone:NO];
        return;
    }
    show_instance();
}
@end

static void observe_instance(void) {
    if (instance_observer) return;
    instance_observer = [[BongoCatInstanceObserver alloc] init];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:instance_observer
        selector:@selector(showWindow:) name:@"com.bongocat.desktop.show" object:nil
        suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
}
BongoCatResult bongo_cat_platform_init(BongoCatPlatform *platform, SDL_Window *window,
    BongoCatInputState *input, BongoCatError *error) {
    memset(platform, 0, sizeof(*platform));
    platform->window = window;
    platform->input = input;
    platform->window_opacity = 1.0f;
    platform->wake_event_type = SDL_RegisterEvents(1);
    if (platform->wake_event_type == (Uint32)-1) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Cannot reserve the macOS input wake event");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    active_platform = platform;
    configure_capture_window(native_window(platform));
    BongoCatError input_error = {0};
    if (!bongo_cat_macos_input_start(platform, &input_error))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", input_error.message);
    if (instance_show_pending) show_instance();
    return BONGO_CAT_OK;
}
void bongo_cat_platform_shutdown(BongoCatPlatform *platform) {
    bongo_cat_macos_input_stop(platform);
    if (active_platform == platform) active_platform = NULL;
}
void bongo_cat_platform_set_click_through(BongoCatPlatform *platform,
    bool forced, bool pointer_transparent) {
    [native_window(platform) setIgnoresMouseEvents:forced || pointer_transparent];
}
bool bongo_cat_platform_set_opacity(BongoCatPlatform *platform, float opacity) {
    if (!platform || !platform->window) return false;
    if (!SDL_SetWindowOpacity(platform->window, opacity)) return false;
    platform->window_opacity = opacity;
    return true;
}
float bongo_cat_platform_get_opacity(const BongoCatPlatform *platform) {
    return platform ? platform->window_opacity : 1.0f;
}
bool bongo_cat_platform_present(BongoCatPlatform *platform, int width, int height) {
    (void)width; (void)height;
    return platform && platform->window && SDL_GL_SwapWindow(platform->window);
}
bool bongo_cat_platform_frame_alpha(const BongoCatPlatform *platform,
    int width, int height, int x, int y, uint8_t *alpha) {
    (void)platform; (void)width; (void)height; (void)x; (void)y; (void)alpha;
    return false;
}
void bongo_cat_platform_set_visible(BongoCatPlatform *platform, bool visible) {
    if (!platform || !platform->window) return;
    visible ? SDL_ShowWindow(platform->window) : SDL_HideWindow(platform->window); if (visible) configure_capture_window(native_window(platform));
}
bool bongo_cat_platform_pointer_local(BongoCatPlatform *platform, double screen_x,
    double screen_y, float *local_x, float *local_y) {
    int x, y, width, height;
    if (!platform || !local_x || !local_y ||
        !SDL_GetWindowPosition(platform->window, &x, &y) ||
        !SDL_GetWindowSize(platform->window, &width, &height)) return false;
    *local_x = (float)(screen_x - x); *local_y = (float)(screen_y - y);
    return *local_x >= 0 && *local_x < width && *local_y >= 0 && *local_y < height;
}
bool bongo_cat_platform_relative_pointer(BongoCatPlatform *platform,
    double *x, double *y) {
    (void)platform; (void)x; (void)y;
    return false;
}
void bongo_cat_platform_relative_pointer_reset(BongoCatPlatform *platform) {
    (void)platform;
}
void bongo_cat_platform_set_always_on_top(BongoCatPlatform *platform, bool enabled) {
    [native_window(platform) setLevel:enabled ? NSFloatingWindowLevel : NSNormalWindowLevel];
}
void bongo_cat_platform_raise_window(SDL_Window *window) {
    if (!window) return;
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    NSWindow *native = (__bridge NSWindow *)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
    configure_capture_window(native);
    [NSApp activateIgnoringOtherApps:YES];
    [native makeKeyAndOrderFront:nil];
}

bool bongo_cat_platform_set_geometry(BongoCatPlatform *platform,
    int x, int y, int width, int height) {
    if (!platform || !platform->window) return false;
    int current_width, current_height;
    if ((!SDL_GetWindowSize(platform->window, &current_width, &current_height) ||
        current_width != width || current_height != height) &&
        !SDL_SetWindowSize(platform->window, width, height)) return false;
    int current_x, current_y;
    if (!SDL_GetWindowPosition(platform->window, &current_x, &current_y) ||
        current_x != x || current_y != y)
        SDL_SetWindowPosition(platform->window, x, y);
    return true;
}
void bongo_cat_platform_begin_drag(BongoCatPlatform *platform,
    BongoCatModalTick modal_tick, void *userdata) {
    NSWindow *window = native_window(platform);
    BongoCatModalTarget *target = modal_tick ? [[BongoCatModalTarget alloc]
        initWithCallback:modal_tick userdata:userdata] : nil;
    if (modal_tick) modal_tick(userdata);
    NSTimer *timer = target ? [NSTimer timerWithTimeInterval:1.0 / 60.0
        target:target selector:@selector(tick:) userInfo:nil repeats:YES] : nil;
    if (timer) [[NSRunLoop currentRunLoop] addTimer:timer
        forMode:NSRunLoopCommonModes];
    [window performWindowDragWithEvent:[NSApp currentEvent]];
    [timer invalidate];
    if (modal_tick) modal_tick(userdata);
    [target release];
}
bool bongo_cat_platform_dynamic_hit_supported(void) {
    return bongo_cat_macos_input_supported();
}

bool bongo_cat_platform_open_directory(const char *path) {
    if (!path || !path[0]) return false;
    @autoreleasepool {
        NSString *directory = [NSString stringWithUTF8String:path];
        return directory && [[NSWorkspace sharedWorkspace]
            openURL:[NSURL fileURLWithPath:directory isDirectory:YES]];
    }
}

void bongo_cat_platform_set_tray_callbacks(void *tray,
    BongoCatTrayClick left_click, BongoCatModalTick modal_tick,
    void *userdata) {
    BongoCatSDLTray *native = tray;
    if (tray_target) {
        [tray_target unbind]; [tray_target release]; tray_target = nil;
    }
    if (!native || !left_click || !native->status_item || !native->menu) return;
    tray_target = [[BongoCatTrayTarget alloc] initWithItem:native->status_item
        menu:native->menu->menu callback:left_click modalTick:modal_tick
        userdata:userdata];
    [native->status_item setMenu:nil];
    [[native->status_item button] setTarget:tray_target];
    [[native->status_item button] setAction:@selector(clicked:)];
    [[native->status_item button] sendActionOn:NSEventMaskLeftMouseUp |
        NSEventMaskRightMouseUp];
}
bool bongo_cat_platform_single_instance_begin(void) {
    char path[96]; snprintf(path, sizeof(path), "/tmp/bongo-cat-%lu.lock",
        (unsigned long)getuid());
    instance_lock = open(path, O_CREAT | O_RDWR, 0600);
    if (instance_lock < 0 || flock(instance_lock, LOCK_EX | LOCK_NB) == 0) {
        observe_instance(); return true;
    }
    [[NSDistributedNotificationCenter defaultCenter]
        postNotificationName:@"com.bongocat.desktop.show" object:nil
        userInfo:nil deliverImmediately:YES];
    close(instance_lock); instance_lock = -1; return false;
}
bool bongo_cat_platform_single_instance_take_wake(void) { return false; }
void bongo_cat_platform_single_instance_end(void) {
    if (instance_lock >= 0) close(instance_lock);
    instance_lock = -1;
    if (instance_observer) {
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:instance_observer];
        [instance_observer release]; instance_observer = nil;
    }
}
BongoCatResult bongo_cat_platform_set_autostart(bool enabled, BongoCatError *error) {
    @autoreleasepool {
        NSString *directory = [NSHomeDirectory()
            stringByAppendingPathComponent:@"Library/LaunchAgents"];
        NSString *path = [directory
            stringByAppendingPathComponent:@"com.bongocat.desktop.plist"];
        NSFileManager *files = [NSFileManager defaultManager];
        if (!enabled) {
            if (![files fileExistsAtPath:path] || [files removeItemAtPath:path error:nil])
                return BONGO_CAT_OK;
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot remove macOS launch agent");
            return BONGO_CAT_ERROR_IO;
        }
        NSString *executable = [[NSBundle mainBundle] executablePath];
        if (!executable || ![files createDirectoryAtPath:directory
            withIntermediateDirectories:YES attributes:nil error:nil]) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot create macOS launch agent directory");
            return BONGO_CAT_ERROR_IO;
        }
        NSDictionary *plist = @{ @"Label": @"com.bongocat.desktop",
            @"ProgramArguments": @[executable, @"--autostart"], @"RunAtLoad": @YES };
        if ([plist writeToFile:path atomically:YES]) return BONGO_CAT_OK;
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO, "Cannot write macOS launch agent");
        return BONGO_CAT_ERROR_IO;
    }
}
BongoCatMenuAction bongo_cat_platform_context_menu(BongoCatPlatform *platform,
    const BongoCatMenuLabels *labels) {
    return bongo_cat_macos_context_menu(platform, labels);
}
BongoCatResult bongo_cat_platform_embedded_assets(const char *target, BongoCatError *error) {
    (void)target; (void)error; return BONGO_CAT_ERROR_PLATFORM;
}
#endif
