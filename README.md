# Roblox Renewal

Windows x64 utility that asks for confirmation, closes Roblox-related processes,
removes `%LOCALAPPDATA%\Roblox` and `%TEMP%\Roblox`, downloads the official
Roblox 64-bit installer, and launches it.

## Usage

Run the executable as a normal user and confirm the warning dialog. Administrator
elevation is not requested because the utility operates on user-writable folders.

## Build

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\build.ps1
```

## Release

- Desktop: `RobloxRenewal.exe`
- Repository release copy: `dist\RobloxRenewal.exe`
