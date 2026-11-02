/*
 * CC18Native pages: a lightweight three-page layer over the untouched iOS 15
 * Control Center.
 *
 *   page 0 - the real system Control Center (visible when the pager is hidden)
 *   page 1 - now-playing player
 *   page 2 - connectivity list
 *
 * A vertical pan switches pages.  The pager is a transparent full-screen view
 * added above the overlay; page 0 does not intercept touches because the pager
 * is hidden until the first upward swipe.
 */

typedef signed char BOOL;
typedef void *id;
typedef void *Class;
typedef void *SEL;
typedef void *IMP;

typedef struct { double x, y; } CGPoint;
typedef struct { double width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;

extern id objc_getClass(const char *name);
extern SEL sel_registerName(const char *name);
extern id objc_msgSend(id self, SEL op, ...);
extern Class objc_allocateClassPair(Class superclass, const char *name, unsigned long extra);
extern void objc_registerClassPair(Class cls);
extern BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);

extern unsigned long strlen(const char *s);

static id g_target;
static id g_overlay;
static id g_pager;
static id g_player_page;
static id g_conn_page;
static id g_player_title;
static id g_player_artist;
static id g_player_art;
static id g_play_button;
static id g_conn_status[7];
static id g_dots[3];
static id g_overlay_pan;
static id g_pager_pan;
static int g_page;

/* ---------- message helpers ---------- */

static id msg0(id o, const char *sel)
{
    return o ? ((id (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel)) : 0;
}

static id msg1(id o, const char *sel, id a)
{
    return o ? ((id (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a) : 0;
}

static id msg2(id o, const char *sel, id a, id b)
{
    return o ? ((id (*)(id, SEL, id, id))objc_msgSend)(o, sel_registerName(sel), a, b) : 0;
}

static id msg_index(id o, const char *sel, unsigned long idx)
{
    return o ? ((id (*)(id, SEL, unsigned long))objc_msgSend)(
                   o, sel_registerName(sel), idx) : 0;
}

static id msg_rect(id o, const char *sel, CGRect r)
{
    return o ? ((id (*)(id, SEL, CGRect))objc_msgSend)(o, sel_registerName(sel), r) : 0;
}

static id msg_size(id o, const char *sel, CGSize s)
{
    return o ? ((id (*)(id, SEL, CGSize))objc_msgSend)(o, sel_registerName(sel), s) : 0;
}

static id msg4d(id o, const char *sel, double a, double b, double c, double d)
{
    return o ? ((id (*)(id, SEL, double, double, double, double))objc_msgSend)(
                   o, sel_registerName(sel), a, b, c, d) : 0;
}

static void msg_void(id o, const char *sel)
{
    if (o) ((void (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel));
}

static void msg_void_id(id o, const char *sel, id a)
{
    if (o) ((void (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_double(id o, const char *sel, double a)
{
    if (o) ((void (*)(id, SEL, double))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_bool(id o, const char *sel, BOOL a)
{
    if (o) ((void (*)(id, SEL, BOOL))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_long(id o, const char *sel, long a)
{
    if (o) ((void (*)(id, SEL, long))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_rect(id o, const char *sel, CGRect r)
{
    if (o) ((void (*)(id, SEL, CGRect))objc_msgSend)(o, sel_registerName(sel), r);
}

static long msg_long(id o, const char *sel)
{
    return o ? ((long (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel)) : 0;
}

static BOOL msg_bool(id o, const char *sel)
{
    return o ? ((BOOL (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel)) : 0;
}

static BOOL responds(id o, const char *sel)
{
    if (!o)
        return 0;
    return ((BOOL (*)(id, SEL, SEL))objc_msgSend)(
        o, sel_registerName("respondsToSelector:"), sel_registerName(sel));
}

static CGRect msg_bounds(id o)
{
    CGRect r = {{0, 0}, {0, 0}};
    return o ? ((CGRect (*)(id, SEL))objc_msgSend)(o, sel_registerName("bounds")) : r;
}

static CGPoint msg_point_id(id o, const char *sel, id a)
{
    CGPoint p = {0, 0};
    return o ? ((CGPoint (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a) : p;
}

static void set_radius(id view, double radius)
{
    id layer = msg0(view, "layer");
    if (layer)
        msg_void_double(layer, "setCornerRadius:", radius);
    msg_void_bool(view, "setClipsToBounds:", 1);
    if (layer)
        msg_void_bool(layer, "setMasksToBounds:", 1);
}

static id str(const char *s)
{
    return msg1((id)objc_getClass("NSString"), "stringWithUTF8String:", (id)s);
}

static id color(double r, double g, double b, double a)
{
    return msg4d((id)objc_getClass("UIColor"), "colorWithRed:green:blue:alpha:", r, g, b, a);
}

static id font(double size, int bold)
{
    Class f = (Class)objc_getClass("UIFont");
    return bold ? ((id (*)(id, SEL, double))objc_msgSend)(
                      (id)f, sel_registerName("boldSystemFontOfSize:"), size)
                : ((id (*)(id, SEL, double))objc_msgSend)(
                      (id)f, sel_registerName("systemFontOfSize:"), size);
}

static id image(const char *name)
{
    return msg1((id)objc_getClass("UIImage"), "systemImageNamed:", str(name));
}

static id new_view(CGRect frame, id bg)
{
    Class UIView = (Class)objc_getClass("UIView");
    id v = msg_rect(msg0((id)UIView, "alloc"), "initWithFrame:", frame);
    if (v && bg)
        msg_void_id(v, "setBackgroundColor:", bg);
    return v;
}

static id new_label(CGRect frame, const char *text, double size, id col, int align)
{
    Class UILabel = (Class)objc_getClass("UILabel");
    id l = msg_rect(msg0((id)UILabel, "alloc"), "initWithFrame:", frame);
    if (!l)
        return 0;
    msg_void_id(l, "setText:", str(text));
    msg_void_id(l, "setTextColor:", col);
    msg_void_id(l, "setFont:", font(size, 0));
    msg_void_long(l, "setTextAlignment:", align);
    msg_void_long(l, "setNumberOfLines:", 0);
    msg_void_id(l, "setBackgroundColor:", msg0((id)objc_getClass("UIColor"), "clearColor"));
    return l;
}

static id new_image_view(CGRect frame, const char *symbol, id tint, double radius)
{
    Class UIImageView = (Class)objc_getClass("UIImageView");
    id iv = msg_rect(msg0((id)UIImageView, "alloc"), "initWithFrame:", frame);
    if (!iv)
        return 0;
    msg_void_id(iv, "setImage:", image(symbol));
    msg_void_id(iv, "setTintColor:", tint);
    msg_void_long(iv, "setContentMode:", 1);
    set_radius(iv, radius);
    return iv;
}

static void add_target(id button, long tag)
{
    msg_void_long(button, "setTag:", tag);
    ((void (*)(id, SEL, id, SEL, unsigned long))objc_msgSend)(
        button, sel_registerName("addTarget:action:forControlEvents:"),
        g_target, sel_registerName("handleButton:"), 64UL);
}

static id new_icon_button(CGRect frame, const char *symbol, long tag)
{
    Class UIButton = (Class)objc_getClass("UIButton");
    id b = ((id (*)(id, SEL, long))objc_msgSend)(
        (id)UIButton, sel_registerName("buttonWithType:"), 1L);
    if (!b)
        return 0;
    msg_void_rect(b, "setFrame:", frame);
    ((void (*)(id, SEL, id, unsigned long))objc_msgSend)(
        b, sel_registerName("setImage:forState:"), image(symbol), 0UL);
    msg_void_id(b, "setTintColor:", color(1, 1, 1, 0.95));
    add_target(b, tag);
    return b;
}

static id new_card(CGRect frame, const char *symbol, const char *title,
                   const char *status, long tag)
{
    Class UIButton = (Class)objc_getClass("UIButton");
    id card = ((id (*)(id, SEL, long))objc_msgSend)(
        (id)UIButton, sel_registerName("buttonWithType:"), 0L);
    if (!card)
        return 0;
    msg_void_rect(card, "setFrame:", frame);
    msg_void_id(card, "setBackgroundColor:", color(1, 1, 1, 0.08));
    set_radius(card, 20);

    id icon = new_image_view((CGRect){{16, 18}, {32, 32}}, symbol,
                             color(1, 1, 1, 0.9), 0);
    if (icon)
        msg_void_id(card, "addSubview:", icon);

    id t = new_label((CGRect){{60, 11}, {frame.size.width - 76, 24}},
                     title, 17, color(1, 1, 1, 0.95), 0);
    if (t)
        msg_void_id(card, "addSubview:", t);

    id s = new_label((CGRect){{60, 36}, {frame.size.width - 76, 20}},
                     status, 13, color(1, 1, 1, 0.55), 0);
    if (s)
        msg_void_id(card, "addSubview:", s);

    add_target(card, tag);
    return card;
}

/* ---------- player ---------- */

static id music_player(void)
{
    Class c = (Class)objc_getClass("MPMusicPlayerController");
    return c ? msg0((id)c, "systemMusicPlayer") : 0;
}

static void update_player(void)
{
    if (!g_player_title)
        return;

    id title = 0;
    id artist = 0;
    id artwork = 0;

    Class center = (Class)objc_getClass("MPNowPlayingInfoCenter");
    if (center) {
        id c = msg0((id)center, "defaultCenter");
        id info = c ? msg0(c, "nowPlayingInfo") : 0;
        if (info) {
            title = msg1(info, "objectForKey:", str("MPMediaItemPropertyTitle"));
            artist = msg1(info, "objectForKey:", str("MPMediaItemPropertyArtist"));
            artwork = msg1(info, "objectForKey:", str("MPMediaItemPropertyArtwork"));
        }
    }

    msg_void_id(g_player_title, "setText:", title ? title : str("未在播放"));
    msg_void_id(g_player_artist, "setText:", artist ? artist : str(""));

    if (artwork && g_player_art) {
        CGSize s = {260, 260};
        id img = msg_size(artwork, "imageWithSize:", s);
        if (img)
            ((void (*)(id, SEL, id))objc_msgSend)(
                g_player_art, sel_registerName("setImage:"), img);
    }

    id p = music_player();
    if (p && g_play_button) {
        long state = msg_long(p, "playbackState");
        const char *sym = (state == 2) ? "pause.fill" : "play.fill";
        ((void (*)(id, SEL, id, unsigned long))objc_msgSend)(
            g_play_button, sel_registerName("setImage:forState:"),
            image(sym), 0UL);
    }
}

static void build_player(CGRect bounds)
{
    g_player_page = new_view(bounds, color(0.025, 0.12, 0.12, 1));

    double w = bounds.size.width;
    double art = w - 110;
    if (art > 300)
        art = 300;
    CGRect art_frame = {{(w - art) / 2.0, 90}, {art, art}};

    id art_bg = new_view(art_frame, color(0.10, 0.35, 0.34, 1));
    set_radius(art_bg, 28);
    if (art_bg)
        msg_void_id(g_player_page, "addSubview:", art_bg);

    g_player_art = new_image_view(art_frame, "music.note",
                                  color(1, 1, 1, 0.55), 28);
    if (g_player_art)
        msg_void_id(art_bg, "addSubview:", g_player_art);

    g_player_title = new_label(
        (CGRect){{30, art_frame.origin.y + art + 24}, {w - 60, 32}},
        "未在播放", 24, color(1, 1, 1, 0.95), 1);
    if (g_player_title)
        msg_void_id(g_player_page, "addSubview:", g_player_title);

    g_player_artist = new_label(
        (CGRect){{30, art_frame.origin.y + art + 58}, {w - 60, 22}},
        "", 14, color(1, 1, 1, 0.55), 1);
    if (g_player_artist)
        msg_void_id(g_player_page, "addSubview:", g_player_artist);

    double by = art_frame.origin.y + art + 100;
    g_play_button = new_icon_button(
        (CGRect){{(w - 64) / 2.0, by}, {64, 64}}, "play.fill", 1);
    id prev = new_icon_button(
        (CGRect){{(w - 64) / 2.0 - 100, by}, {64, 64}}, "backward.fill", 2);
    id next = new_icon_button(
        (CGRect){{(w - 64) / 2.0 + 100, by}, {64, 64}}, "forward.fill", 3);
    if (g_play_button)
        msg_void_id(g_player_page, "addSubview:", g_play_button);
    if (prev)
        msg_void_id(g_player_page, "addSubview:", prev);
    if (next)
        msg_void_id(g_player_page, "addSubview:", next);

    Class MPVolumeView = (Class)objc_getClass("MPVolumeView");
    if (MPVolumeView) {
        id vol = msg_rect(msg0((id)MPVolumeView, "alloc"),
                          "initWithFrame:",
                          (CGRect){{30, bounds.size.height - 90},
                                   {w - 60, 40}});
        if (vol) {
            msg_void_bool(vol, "setShowsRouteButton:", 0);
            msg_void_id(g_player_page, "addSubview:", vol);
        }
    }

    msg_void_bool(g_player_page, "setHidden:", 1);
}

/* ---------- connectivity ---------- */

static BOOL wifi_state(BOOL *known)
{
    Class c = (Class)objc_getClass("SBWiFiManager");
    if (!c) {
        *known = 0;
        return 0;
    }
    id m = msg0((id)c, "sharedInstance");
    if (!m || !responds(m, "isWiFiEnabled")) {
        *known = 0;
        return 0;
    }
    *known = 1;
    return msg_bool(m, "isWiFiEnabled");
}

static BOOL bt_state(BOOL *known)
{
    Class c = (Class)objc_getClass("BluetoothManager");
    if (!c) {
        *known = 0;
        return 0;
    }
    id m = msg0((id)c, "sharedInstance");
    if (!m || !responds(m, "enabled")) {
        *known = 0;
        return 0;
    }
    *known = 1;
    return msg_bool(m, "enabled");
}

static BOOL airplane_state(BOOL *known)
{
    Class c = (Class)objc_getClass("RadiosPreferences");
    if (!c) {
        *known = 0;
        return 0;
    }
    id r = msg0(msg0((id)c, "alloc"), "init");
    if (!r || !responds(r, "airplaneMode")) {
        *known = 0;
        return 0;
    }
    *known = 1;
    return msg_bool(r, "airplaneMode");
}

static void toggle_wifi(void)
{
    Class c = (Class)objc_getClass("SBWiFiManager");
    id m = c ? msg0((id)c, "sharedInstance") : 0;
    if (m && responds(m, "isWiFiEnabled") && responds(m, "setWiFiEnabled:"))
        msg_void_bool(m, "setWiFiEnabled:", !msg_bool(m, "isWiFiEnabled"));
}

static void toggle_bt(void)
{
    Class c = (Class)objc_getClass("BluetoothManager");
    id m = c ? msg0((id)c, "sharedInstance") : 0;
    if (m && responds(m, "enabled") && responds(m, "setEnabled:"))
        msg_void_bool(m, "setEnabled:", !msg_bool(m, "enabled"));
}

static void toggle_airplane(void)
{
    Class c = (Class)objc_getClass("RadiosPreferences");
    id r = c ? msg0(msg0((id)c, "alloc"), "init") : 0;
    if (r && responds(r, "airplaneMode") && responds(r, "setAirplaneMode:")) {
        msg_void_bool(r, "setAirplaneMode:", !msg_bool(r, "airplaneMode"));
        if (responds(r, "synchronize"))
            msg_void(r, "synchronize");
    }
}

static void open_settings(const char *path)
{
    Class UIApplication = (Class)objc_getClass("UIApplication");
    Class NSURL = (Class)objc_getClass("NSURL");
    id app = UIApplication ? msg0((id)UIApplication, "sharedApplication") : 0;
    id url = NSURL ? msg1((id)NSURL, "URLWithString:", str(path)) : 0;
    if (app && url)
        msg_void_id(app, "openURL:", url);
}

static void update_conn(void)
{
    BOOL known;
    BOOL on;

    on = airplane_state(&known);
    if (g_conn_status[0])
        msg_void_id(g_conn_status[0], "setText:",
                    known ? str(on ? "开启" : "关闭") : str("不可用"));

    on = wifi_state(&known);
    if (g_conn_status[1])
        msg_void_id(g_conn_status[1], "setText:",
                    known ? str(on ? "已开启" : "未连接") : str("不可用"));

    on = bt_state(&known);
    if (g_conn_status[4])
        msg_void_id(g_conn_status[4], "setText:",
                    known ? str(on ? "开启" : "关闭") : str("不可用"));
}

static void build_conn(CGRect bounds)
{
    g_conn_page = new_view(bounds, color(0.025, 0.12, 0.12, 1));

    double w = bounds.size.width;
    double card_w = w - 70;
    double y = 72;
    const char *icons[7] = {
        "airplane", "wifi", "dot.radiowaves.left.and.right",
        "antenna.radiowaves.left.and.right", "bluetooth",
        "personalhotspot", "lock.shield"
    };
    const char *titles[7] = {
        "飞行模式", "WLAN", "AirDrop", "蜂窝数据",
        "蓝牙", "个人热点", "VPN"
    };
    const char *status[7] = {
        "关闭", "未连接", "仅联系人", "开启", "开启", "不可发现", "关闭"
    };

    for (int i = 0; i < 7; i++) {
        CGRect f = {{35, y}, {card_w, 64}};
        id card = new_card(f, icons[i], titles[i], status[i], 100 + i);
        if (!card)
            continue;
        msg_void_id(g_conn_page, "addSubview:", card);
        /* status labels are the only UILabel subviews after the title */
        id subs = msg0(card, "subviews");
        unsigned long n = subs ? msg_long(subs, "count") : 0;
        if (n >= 3)
            g_conn_status[i] = msg_index(subs, "objectAtIndex:", n - 1);
        y += 72;
    }

    msg_void_bool(g_conn_page, "setHidden:", 1);
}

/* ---------- pager ---------- */

static void update_dots(void)
{
    for (int i = 0; i < 3; i++) {
        if (!g_dots[i])
            continue;
        msg_void_id(g_dots[i], "setBackgroundColor:",
                    color(1, 1, 1, (i + 1) == g_page ? 0.95 : 0.25));
    }
}

static void show_page(int page)
{
    if (!g_pager)
        return;
    g_page = page;
    if (g_overlay_pan)
        msg_void_bool(g_overlay_pan, "setEnabled:", page == 0);
    if (g_pager_pan)
        msg_void_bool(g_pager_pan, "setEnabled:", page != 0);
    if (page == 0) {
        msg_void_bool(g_pager, "setHidden:", 1);
        return;
    }
    msg_void_bool(g_pager, "setHidden:", 0);
    msg_void_bool(g_player_page, "setHidden:", page != 1);
    msg_void_bool(g_conn_page, "setHidden:", page != 2);
    update_dots();
    if (page == 1)
        update_player();
    if (page == 2)
        update_conn();
}

static void handle_pan(id self, SEL _cmd, id gesture)
{
    id host = msg0(gesture, "view");
    if (host == g_pager && g_page == 0)
        return;
    if (host != g_pager && g_page != 0)
        return;
    if (msg_long(gesture, "state") != 3)
        return;
    CGPoint t = msg_point_id(gesture, "translationInView:", g_pager);
    if (t.y < -55) {
        if (g_page == 0)
            show_page(1);
        else if (g_page == 1)
            show_page(2);
    } else if (t.y > 55) {
        if (g_page == 2)
            show_page(1);
        else if (g_page == 1)
            show_page(0);
    }
}

static void handle_tick(id self, SEL _cmd, id timer)
{
    if (g_page == 1)
        update_player();
}

static void handle_button(id self, SEL _cmd, id button)
{
    long tag = msg_long(button, "tag");
    if (tag == 1) {
        id p = music_player();
        if (p) {
            long state = msg_long(p, "playbackState");
            if (state == 2)
                msg_void(p, "pause");
            else
                msg_void(p, "play");
        }
        update_player();
    } else if (tag == 2) {
        id p = music_player();
        if (p)
            msg_void(p, "skipToPreviousItem");
        update_player();
    } else if (tag == 3) {
        id p = music_player();
        if (p)
            msg_void(p, "skipToNextItem");
        update_player();
    } else if (tag == 100) {
        toggle_airplane();
        update_conn();
    } else if (tag == 101) {
        toggle_wifi();
        update_conn();
    } else if (tag == 102) {
        open_settings("prefs:root=AIRDROP");
    } else if (tag == 103) {
        open_settings("prefs:root=MOBILE_DATA_SETTINGS_ID");
    } else if (tag == 104) {
        toggle_bt();
        update_conn();
    } else if (tag == 105) {
        open_settings("prefs:root=INTERNET_TETHERING");
    } else if (tag == 106) {
        open_settings("prefs:root=VPN");
    }
}

static void ensure_target(void)
{
    if (g_target)
        return;
    Class cls = objc_allocateClassPair((Class)objc_getClass("NSObject"),
                                       "CC18NativeTarget", 0);
    if (!cls)
        return;
    class_addMethod(cls, sel_registerName("handleButton:"), (IMP)handle_button, "v@:@");
    class_addMethod(cls, sel_registerName("handlePan:"), (IMP)handle_pan, "v@:@");
    class_addMethod(cls, sel_registerName("tick:"), (IMP)handle_tick, "v@:@");
    objc_registerClassPair(cls);
    g_target = msg0(msg0((id)cls, "alloc"), "init");
}

static id add_pan(id view)
{
    Class UIPan = (Class)objc_getClass("UIPanGestureRecognizer");
    if (!UIPan)
        return 0;
    id g = msg2(msg0((id)UIPan, "alloc"), "initWithTarget:action:",
                g_target, (id)sel_registerName("handlePan:"));
    if (g) {
        msg_void_bool(g, "setCancelsTouchesInView:", 0);
        msg_void_id(view, "addGestureRecognizer:", g);
    }
    return g;
}

static void layout_pages(void)
{
    if (!g_pager || !g_overlay)
        return;
    CGRect b = msg_bounds(g_overlay);
    msg_void_rect(g_pager, "setFrame:", b);
    msg_void_rect(g_player_page, "setFrame:", b);
    msg_void_rect(g_conn_page, "setFrame:", b);
    double x = b.size.width - 18;
    double y = b.size.height / 2.0 - 30;
    for (int i = 0; i < 3; i++) {
        if (g_dots[i])
            msg_void_rect(g_dots[i], "setFrame:",
                          (CGRect){{x, y + i * 26}, {8, 8}});
    }
}

void cc18_pages_layout(id overlay_view)
{
    if (!overlay_view)
        return;
    g_overlay = overlay_view;
    layout_pages();
}

void cc18_pages_attach(id overlay_view)
{
    if (g_pager || !overlay_view)
        return;
    ensure_target();
    if (!g_target)
        return;

    g_overlay = overlay_view;
    CGRect b = msg_bounds(overlay_view);
    g_pager = new_view(b, 0);
    if (!g_pager)
        return;
    msg_void_bool(g_pager, "setHidden:", 1);

    build_player(b);
    build_conn(b);
    if (g_player_page)
        msg_void_id(g_pager, "addSubview:", g_player_page);
    if (g_conn_page)
        msg_void_id(g_pager, "addSubview:", g_conn_page);

    for (int i = 0; i < 3; i++) {
        g_dots[i] = new_view((CGRect){{0, 0}, {8, 8}}, color(1, 1, 1, 0.25));
        set_radius(g_dots[i], 4);
        if (g_dots[i])
            msg_void_id(g_pager, "addSubview:", g_dots[i]);
    }

    msg_void_id(overlay_view, "addSubview:", g_pager);
    g_overlay_pan = add_pan(overlay_view);
    g_pager_pan = add_pan(g_pager);
    if (g_pager_pan)
        msg_void_bool(g_pager_pan, "setEnabled:", 1);
    if (g_overlay_pan)
        msg_void_bool(g_overlay_pan, "setEnabled:", 1);
    layout_pages();
    update_dots();
}
