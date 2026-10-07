#include "macos_internal.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <stdio.h>

@interface BongoCatMenuTarget : NSObject <NSMenuDelegate> {
    NSInteger selected_; const BongoCatMenuLabels *labels_;
}
- (id)initWithLabels:(const BongoCatMenuLabels *)labels;
- (void)choose:(id)sender;
- (NSInteger)selected;
- (void)tickPreview:(NSTimer *)timer;
@end

@implementation BongoCatMenuTarget
- (id)initWithLabels:(const BongoCatMenuLabels *)labels {
    self = [super init]; if (self) labels_ = labels; return self;
}
- (void)choose:(id)sender { selected_ = [sender tag]; }
- (NSInteger)selected { return selected_; }
- (void)tickPreview:(NSTimer *)timer {
    (void)timer;
    if (labels_->preview_tick) labels_->preview_tick(labels_->preview_userdata);
}
- (void)menu:(NSMenu *)menu willHighlightItem:(NSMenuItem *)item {
    (void)menu;
    if (labels_->preview) labels_->preview(labels_->preview_userdata,
        item ? (BongoCatMenuAction)[item tag] : BONGO_CAT_MENU_NONE);
}
@end

static NSString *text(const char *value) {
    return value ? [NSString stringWithUTF8String:value] : @"";
}

static NSMenuItem *add_item(NSMenu *menu, BongoCatMenuTarget *target,
    const char *label, NSInteger tag, bool checked) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:text(label)
        action:@selector(choose:) keyEquivalent:@""];
    [item setTarget:target]; [item setTag:tag];
    [item setState:checked ? NSControlStateValueOn : NSControlStateValueOff];
    [menu addItem:item]; [item release]; return item;
}

static void add_scale_menu(NSMenu *menu, BongoCatMenuTarget *target,
    const char *label, const char *hint_label, bool opacity) {
    NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:text(label)
        action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:text(label)];
    [submenu setDelegate:target];
    const int values[] = {10,20,30,40,50,60,70,80,90,100};
    int count = opacity ? 10 : 16;
    for (int i = 0; i < count; ++i) {
        int value = opacity ? values[i] : 50 + i * 10;
        char title[16]; snprintf(title, sizeof(title), "%d%%", value);
        NSInteger tag = opacity ? BONGO_CAT_MENU_OPACITY_10 + i : BONGO_CAT_MENU_SCALE_50 + i;
        add_item(submenu, target, title, tag, false);
    }
    NSMenuItem *hint = [[NSMenuItem alloc] initWithTitle:text(hint_label)
        action:nil keyEquivalent:@""];
    [hint setEnabled:NO]; [submenu addItem:hint]; [hint release];
    [root setSubmenu:submenu]; [menu addItem:root];
    [submenu release]; [root release];
}

static void add_named_menu(NSMenu *menu, BongoCatMenuTarget *target,
    const char *label, const char *const *names, size_t count, NSInteger first,
    size_t selected) {
    if (!count) return;
    NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:text(label)
        action:nil keyEquivalent:@""];
    NSMenu *submenu = [[NSMenu alloc] initWithTitle:text(label)];
    [submenu setDelegate:target];
    for (size_t i = 0; i < count; ++i)
        add_item(submenu, target, names[i], first + (NSInteger)i, i == selected);
    [root setSubmenu:submenu]; [menu addItem:root];
    [submenu release]; [root release];
}

BongoCatMenuAction bongo_cat_macos_context_menu(BongoCatPlatform *platform,
    const BongoCatMenuLabels *labels) {
    (void)platform;
    if (!labels) return BONGO_CAT_MENU_NONE;
    BongoCatMenuTarget *target = [[BongoCatMenuTarget alloc] initWithLabels:labels];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [menu setDelegate:target];
    add_item(menu, target, labels->preferences, BONGO_CAT_MENU_PREFERENCES, false);
    add_item(menu, target, labels->hide, BONGO_CAT_MENU_HIDE, false);
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->pass_through, BONGO_CAT_MENU_PASS_THROUGH,
        labels->pass_through_checked);
    add_item(menu, target, labels->always_on_top, BONGO_CAT_MENU_ALWAYS_ON_TOP,
        labels->always_on_top_checked);
    add_scale_menu(menu, target, labels->window_size, labels->wheel_size_hint, false);
    add_scale_menu(menu, target, labels->opacity, labels->wheel_opacity_hint, true);
    add_named_menu(menu, target, labels->motion, labels->motion_names,
        labels->motion_count, BONGO_CAT_MENU_MOTION_FIRST, (size_t)-1);
    add_named_menu(menu, target, labels->expression, labels->expression_names,
        labels->expression_count, BONGO_CAT_MENU_EXPRESSION_FIRST,
        labels->current_expression);
    NSMenuItem *modelRoot = [[NSMenuItem alloc] initWithTitle:text(labels->model)
        action:nil keyEquivalent:@""];
    NSMenu *models = [[NSMenu alloc] initWithTitle:text(labels->model)];
    [models setDelegate:target];
    for (size_t i = 0; i < labels->model_count; ++i)
        add_item(models, target, labels->model_names[i], BONGO_CAT_MENU_MODEL_FIRST + i,
            i == labels->current_model);
    if (labels->model_count) [models addItem:[NSMenuItem separatorItem]];
    add_item(models, target, labels->add_model, BONGO_CAT_MENU_MODEL_ADD, false);
    [modelRoot setSubmenu:models]; [menu addItem:modelRoot];
    [models release]; [modelRoot release];
    [menu addItem:[NSMenuItem separatorItem]];
    add_item(menu, target, labels->exit, BONGO_CAT_MENU_EXIT, false);
    NSTimer *previewTimer = nil;
    if (labels->preview_tick) {
        previewTimer = [NSTimer timerWithTimeInterval:1.0 / 60.0 target:target
            selector:@selector(tickPreview:) userInfo:nil repeats:YES];
        [[NSRunLoop currentRunLoop] addTimer:previewTimer forMode:NSRunLoopCommonModes];
    }
    [menu popUpMenuPositioningItem:nil atLocation:[NSEvent mouseLocation] inView:nil];
    [previewTimer invalidate];
    BongoCatMenuAction result = (BongoCatMenuAction)[target selected];
    if (labels->restore) labels->restore(labels->preview_userdata, result);
    [menu release]; [target release]; return result;
}
#endif
