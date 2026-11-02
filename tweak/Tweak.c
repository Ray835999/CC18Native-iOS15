/*
 * CC18Native - iOS 15 native visual pass for the iOS 18 Control Center look.
 *
 * This does not load ControlCenter18's iOS 16 binary.  It hooks the real
 * iOS 15 Control Center classes and restyles their existing views:
 *
 *   - CCUIContentModuleContainerView  -> 26pt continuous corners + border
 *   - CCUIRoundButton                 -> circular corners + border
 *   - material/background subviews    -> matching corners
 *
 * Hooks are installed when ControlCenterUI/ControlCenterUIKit are mapped, and
 * the overlay is restyled again from SBControlCenterController -_didPresent
 * and the overlay's layout callbacks, so modules created later are covered.
 *
 * Build: clang -target arm64-apple-ios15.0 -dynamiclib -nostdlib ...
 */

typedef signed char BOOL;
typedef void *id;
typedef void *Class;
typedef void *SEL;
typedef void *IMP;
typedef void *Method;

typedef struct { double x, y; } CGPoint;
typedef struct { double width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;

extern id objc_getClass(const char *name);
extern Class object_getClass(id object);
extern const char *class_getName(Class cls);
extern SEL sel_registerName(const char *name);
extern id objc_msgSend(id self, SEL op, ...);
extern Method class_getInstanceMethod(Class cls, SEL sel);
extern BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);
extern const char *method_getTypeEncoding(Method m);
extern IMP method_getImplementation(Method m);
extern IMP method_setImplementation(Method m, IMP imp);

typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;

extern void _dyld_register_func_for_add_image(void (*func)(const void *mh, long slide));
extern int dladdr(const void *addr, Dl_info *info);

extern int open(const char *path, int flags, ...);
extern long write(int fd, const void *buf, unsigned long n);
extern int close(int fd);
extern unsigned long strlen(const char *s);
extern char *strstr(const char *haystack, const char *needle);

#define O_WRONLY 0x0001
#define O_CREAT 0x0200
#define O_APPEND 0x0008

#define TAG_BORDER 0x43433138  /* "CC18" */

static int g_container_hooked;
static int g_round_hooked;
static int g_overlay_hooked;
static int g_collection_hooked;
static int g_sb_hooked;

extern void cc18_pages_attach(id overlay_view);
extern void cc18_pages_layout(id overlay_view);

static IMP orig_container_layout;
static IMP orig_round_layout;
static IMP orig_overlay_viewdidload;
static IMP orig_overlay_layout;
static IMP orig_collection_appear;
static IMP orig_did_present;

static id curve_string;

static void nat_log(const char *msg)
{
    int fd = open("/var/mobile/cc18native.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return;
    write(fd, msg, strlen(msg));
    write(fd, "\n", 1);
    close(fd);
}

static id msg0(id o, const char *sel)
{
    if (!o)
        return 0;
    return ((id (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel));
}

static id msg1(id o, const char *sel, id a)
{
    if (!o)
        return 0;
    return ((id (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a);
}

static id msg_rect(id o, const char *sel, CGRect r)
{
    if (!o)
        return 0;
    return ((id (*)(id, SEL, CGRect))objc_msgSend)(o, sel_registerName(sel), r);
}

static id msg_dd(id o, const char *sel, double a, double b)
{
    if (!o)
        return 0;
    return ((id (*)(id, SEL, double, double))objc_msgSend)(o, sel_registerName(sel), a, b);
}

static unsigned long msg_ulong(id o, const char *sel)
{
    if (!o)
        return 0;
    return ((unsigned long (*)(id, SEL))objc_msgSend)(o, sel_registerName(sel));
}

static id msg_index(id o, const char *sel, unsigned long idx)
{
    if (!o)
        return 0;
    return ((id (*)(id, SEL, unsigned long))objc_msgSend)(o, sel_registerName(sel), idx);
}

static void msg_void_id(id o, const char *sel, id a)
{
    if (!o)
        return;
    ((void (*)(id, SEL, id))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_double(id o, const char *sel, double a)
{
    if (!o)
        return;
    ((void (*)(id, SEL, double))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_bool(id o, const char *sel, BOOL a)
{
    if (!o)
        return;
    ((void (*)(id, SEL, BOOL))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_long(id o, const char *sel, long a)
{
    if (!o)
        return;
    ((void (*)(id, SEL, long))objc_msgSend)(o, sel_registerName(sel), a);
}

static void msg_void_rect(id o, const char *sel, CGRect r)
{
    if (!o)
        return;
    ((void (*)(id, SEL, CGRect))objc_msgSend)(o, sel_registerName(sel), r);
}

static CGRect msg_bounds(id o)
{
    CGRect r = {{0, 0}, {0, 0}};
    if (!o)
        return r;
    return ((CGRect (*)(id, SEL))objc_msgSend)(o, sel_registerName("bounds"));
}

static BOOL has_method(id object, const char *name)
{
    Class cls = object ? object_getClass(object) : 0;
    return cls && class_getInstanceMethod(cls, sel_registerName(name)) != 0;
}

static BOOL hook_method(Class cls, const char *name, IMP replacement, IMP *original)
{
    if (!cls)
        return 0;
    SEL sel = sel_registerName(name);
    Method method = class_getInstanceMethod(cls, sel);
    if (!method)
        return 0;
    const char *types = method_getTypeEncoding(method);
    if (class_addMethod(cls, sel, replacement, types)) {
        if (original)
            *original = method_getImplementation(method);
        return 1;
    }
    if (original)
        *original = method_setImplementation(method, replacement);
    return 1;
}

static id make_curve(void)
{
    if (curve_string)
        return curve_string;
    Class NSString = (Class)objc_getClass("NSString");
    if (NSString) {
        id alloc = msg0((id)NSString, "alloc");
        if (alloc)
            curve_string = msg1(alloc, "initWithUTF8String:", (id)"continuous");
    }
    return curve_string;
}

static void set_layer_style(id view, double radius, double border)
{
    if (!view)
        return;
    id layer = msg0(view, "layer");
    msg_void_double(layer, "setCornerRadius:", radius);
    id curve = make_curve();
    if (curve)
        msg_void_id(layer, "setCornerCurve:", curve);
    msg_void_bool(view, "setClipsToBounds:", 1);
    msg_void_bool(layer, "setMasksToBounds:", 1);
    if (border > 0) {
        msg_void_double(layer, "setBorderWidth:", border);
        id color = msg_dd((id)objc_getClass("UIColor"),
                          "colorWithWhite:alpha:", 1.0, 0.35);
        if (color) {
            id cg = msg0(color, "CGColor");
            msg_void_id(layer, "setBorderColor:", cg);
        }
    }
}

static void ensure_border(id view, double radius)
{
    if (!view)
        return;
    id overlay = msg_index(view, "viewWithTag:", TAG_BORDER);
    if (!overlay) {
        Class UIView = (Class)objc_getClass("UIView");
        if (!UIView)
            return;
        CGRect b = msg_bounds(view);
        CGRect frame = {{0, 0}, {b.size.width, b.size.height}};
        overlay = msg_rect(msg0((id)UIView, "alloc"), "initWithFrame:", frame);
        if (!overlay)
            return;
        msg_void_long(overlay, "setTag:", TAG_BORDER);
        msg_void_bool(overlay, "setUserInteractionEnabled:", 0);
        msg_void_long(overlay, "setAutoresizingMask:", 18);
        msg_void_id(view, "addSubview:", overlay);
    }

    CGRect b = msg_bounds(view);
    CGRect frame = {{0, 0}, {b.size.width, b.size.height}};
    msg_void_rect(overlay, "setFrame:", frame);
    id layer = msg0(overlay, "layer");
    msg_void_double(layer, "setCornerRadius:", radius);
    msg_void_bool(layer, "setMasksToBounds:", 1);
    msg_void_double(layer, "setBorderWidth:", 2.0);
    id color = msg_dd((id)objc_getClass("UIColor"),
                      "colorWithWhite:alpha:", 1.0, 0.35);
    if (color) {
        id cg = msg0(color, "CGColor");
        msg_void_id(layer, "setBorderColor:", cg);
    }
    msg_void_id(view, "bringSubviewToFront:", overlay);
}

static void style_material_children(id view, double radius, int depth)
{
    if (!view || depth > 4)
        return;
    id subviews = msg0(view, "subviews");
    unsigned long count = msg_ulong(subviews, "count");
    for (unsigned long i = 0; i < count; i++) {
        id child = msg_index(subviews, "objectAtIndex:", i);
        if (!child)
            continue;
        const char *name = class_getName(object_getClass(child));
        if (name && (strstr(name, "MaterialView") ||
                     strstr(name, "VisualEffectView") ||
                     strstr(name, "BackgroundView"))) {
            set_layer_style(child, radius, 0);
        }
        style_material_children(child, radius, depth + 1);
    }
}

static double module_radius(id view, double fallback)
{
    CGRect b = msg_bounds(view);
    double m = b.size.width < b.size.height ? b.size.width : b.size.height;
    if (m <= 1.0)
        return fallback;
    /* Large 2x2 tiles are rounded squares, not circles.  Small 1x1 buttons
     * stay circular; elongated 2x1/1x2 tiles become capsules. */
    if (m >= 110.0)
        return 28.0;
    return m / 2.0;
}

static void style_container(id view)
{
    if (!view)
        return;
    double radius = module_radius(view, 26.0);
    set_layer_style(view, radius, 0);
    id inner = has_method(view, "containerView") ? msg0(view, "containerView") : 0;
    if (inner)
        set_layer_style(inner, radius, 0);
    id ca = has_method(view, "caAnimationContainerView") ? msg0(view, "caAnimationContainerView") : 0;
    if (ca)
        set_layer_style(ca, radius, 0);
    id c2 = has_method(view, "c2AnimationContainerView") ? msg0(view, "c2AnimationContainerView") : 0;
    if (c2)
        set_layer_style(c2, radius, 0);
    style_material_children(view, radius, 0);
}

static void style_round(id view)
{
    if (!view)
        return;
    double radius = module_radius(view, 22.0);
    set_layer_style(view, radius, 0);

    const char *props[] = {
        "normalStateBackgroundView",
        "selectedStateBackgroundView",
        "alternateSelectedStateBackgroundView",
        0
    };
    for (int i = 0; props[i]; i++) {
        id bg = has_method(view, props[i]) ? msg0(view, props[i]) : 0;
        if (bg)
            set_layer_style(bg, radius, 0);
    }
}

static void style_tree(id view, int depth)
{
    if (!view || depth > 14)
        return;
    Class cls = object_getClass(view);
    const char *name = cls ? class_getName(cls) : 0;
    if (name) {
        if (strstr(name, "CCUIContentModuleContainerView"))
            style_container(view);
        else if (strstr(name, "CCUIRoundButton"))
            style_round(view);
    }
    id subviews = msg0(view, "subviews");
    unsigned long count = msg_ulong(subviews, "count");
    for (unsigned long i = 0; i < count; i++)
        style_tree(msg_index(subviews, "objectAtIndex:", i), depth + 1);
}

static void container_layout(id self, SEL _cmd)
{
    if (orig_container_layout)
        ((void (*)(id, SEL))orig_container_layout)(self, _cmd);
    style_container(self);
}

static void round_layout(id self, SEL _cmd)
{
    if (orig_round_layout)
        ((void (*)(id, SEL))orig_round_layout)(self, _cmd);
    style_round(self);
}

static void overlay_layout(id self, SEL _cmd)
{
    if (orig_overlay_layout)
        ((void (*)(id, SEL))orig_overlay_layout)(self, _cmd);
    id view = msg0(self, "view");
    style_tree(view, 0);
    cc18_pages_layout(view);
}

static void overlay_viewdidload(id self, SEL _cmd)
{
    if (orig_overlay_viewdidload)
        ((void (*)(id, SEL))orig_overlay_viewdidload)(self, _cmd);
    id view = msg0(self, "view");
    style_tree(view, 0);
    cc18_pages_attach(view);
}

static void collection_appear(id self, SEL _cmd, BOOL animated)
{
    if (orig_collection_appear)
        ((void (*)(id, SEL, BOOL))orig_collection_appear)(self, _cmd, animated);
    style_tree(msg0(self, "view"), 0);
}

static void style_all_windows(void)
{
    id app = msg0((id)objc_getClass("UIApplication"), "sharedApplication");
    id windows = msg0(app, "windows");
    unsigned long count = msg_ulong(windows, "count");
    for (unsigned long i = 0; i < count; i++)
        style_tree(msg_index(windows, "objectAtIndex:", i), 0);
}

static void cc_install(void)
{
    if (!g_container_hooked) {
        Class c = (Class)objc_getClass("CCUIContentModuleContainerView");
        if (c && hook_method(c, "layoutSubviews", (IMP)container_layout, &orig_container_layout)) {
            g_container_hooked = 1;
            nat_log("CC18Native: container hooked");
        }
    }
    if (!g_round_hooked) {
        Class c = (Class)objc_getClass("CCUIRoundButton");
        if (c && hook_method(c, "layoutSubviews", (IMP)round_layout, &orig_round_layout)) {
            g_round_hooked = 1;
            nat_log("CC18Native: round button hooked");
        }
    }
    if (!g_overlay_hooked) {
        Class c = (Class)objc_getClass("CCUIModularControlCenterOverlayViewController");
        if (c) {
            int a = hook_method(c, "viewDidLoad", (IMP)overlay_viewdidload, &orig_overlay_viewdidload);
            int b = hook_method(c, "viewWillLayoutSubviews", (IMP)overlay_layout, &orig_overlay_layout);
            if (a || b) {
                g_overlay_hooked = 1;
                nat_log("CC18Native: overlay hooked");
            }
        }
    }
    if (!g_collection_hooked) {
        Class c = (Class)objc_getClass("CCUIModuleCollectionViewController");
        if (c && hook_method(c, "viewWillAppear:", (IMP)collection_appear, &orig_collection_appear)) {
            g_collection_hooked = 1;
            nat_log("CC18Native: collection hooked");
        }
    }
}

static void did_present(id self, SEL _cmd)
{
    if (orig_did_present)
        ((void (*)(id, SEL))orig_did_present)(self, _cmd);
    cc_install();
    id vc = msg0(self, "viewController");
    if (vc) {
        id view = msg0(vc, "view");
        style_tree(view, 0);
        cc18_pages_attach(view);
        cc18_pages_layout(view);
    } else {
        style_all_windows();
    }
}

static void install_sb_hook(void)
{
    if (g_sb_hooked)
        return;
    Class c = (Class)objc_getClass("SBControlCenterController");
    if (c && hook_method(c, "_didPresent", (IMP)did_present, &orig_did_present)) {
        g_sb_hooked = 1;
        nat_log("CC18Native: SBControlCenterController hooked");
    }
}

static void image_added(const void *mh, long slide)
{
    Dl_info info;
    if (mh && dladdr(mh, &info) && info.dli_fname) {
        if (strstr(info.dli_fname, "ControlCenter"))
            cc_install();
    }
    install_sb_hook();
}

__attribute__((constructor)) static void cc18native_init(void)
{
    nat_log("CC18Native: constructor");
    install_sb_hook();
    cc_install();
    _dyld_register_func_for_add_image(image_added);
}
