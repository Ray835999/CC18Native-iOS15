# CC18 Native (iOS 15)

A native iOS 15 visual pass for the iOS 18 Control Center look.  This does **not**
load the iOS 16-only ControlCenter18 binary.

## v0.1

- `CCUIContentModuleContainerView -layoutSubviews`
  - 26pt continuous corner radius
  - clips content
  - 2px translucent border
- `CCUIRoundButton -layoutSubviews`
  - circular corner radius
  - circular background views
  - 2px border
- `CCUIModularControlCenterOverlayViewController` + `CCUIModuleCollectionViewController`
  - walk the live view tree and style modules created after the hooks were installed
- `SBControlCenterController -_didPresent`
  - installs the CC hooks and forces one styling pass over the presented overlay

All hooks are installed lazily when `ControlCenterUI` / `ControlCenterUIKit` are
mapped, so the tweak does not depend on private classes being present in the
SpringBoard executable at constructor time.

Log: `/var/mobile/cc18native.log`

## Build

GitHub Actions on Ubuntu Linux, no macOS:

```sh
clang -target arm64-apple-ios15.0 -dynamiclib -nostdlib -fuse-ld=lld \
  -Wl,-platform_version,ios,15.0,15.0 \
  -Wl,-install_name,/var/jb/Library/MobileSubstrate/DynamicLibraries/CC18Native.dylib \
  -Wl,-undefined,dynamic_lookup tweak/Tweak.c -o CC18Native.dylib
```

The workflow builds arm64 + arm64e, ad-hoc signs both slices, and packs a
rootless `iphoneos-arm64` deb.

## Install

Rootless iOS 15 (`/var/jb`), Dopamine/ElleKit or CydiaSubstrate.  The postinst
respring.  If the controls do not change, check `/var/mobile/cc18native.log`
for the `container hooked` / `round button hooked` / `overlay hooked` lines.
