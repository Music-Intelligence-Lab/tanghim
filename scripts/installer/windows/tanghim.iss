; Tanghim installer — Inno Setup 6 script.
; Invoked by the release workflow:
;   ISCC.exe /DMyAppVersion=<version> /DPluginsDir=<abs-path> /DOutputDir=<abs-path> tanghim.iss
;
; PluginsDir must contain:
;   Tanghim.vst3\
;   Tanghim.clap
;   Tanghim Receiver.vst3\
;   Tanghim Receiver.clap
;   Tanghim MPE Receiver.amxd
;   Tanghim Mono PB Receiver.amxd
;   MTS-ESP-Max-Package\

#ifndef MyAppVersion
  #error MyAppVersion is required (pass /DMyAppVersion=...)
#endif
#ifndef PluginsDir
  #error PluginsDir is required (pass /DPluginsDir=...)
#endif
#ifndef OutputDir
  #error OutputDir is required (pass /DOutputDir=...)
#endif

#define MyAppName "Tanghim"
#define MyAppPublisher "KhyamAllami"

[Setup]
AppId={{D7F4A1C2-6E3B-4F8A-9B5C-1A2E8F9D0C3B}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Tanghim-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Uninstallable=no
CreateUninstallRegKey=no
WizardStyle=modern

[Components]
Name: "transmitter"; Description: "Tanghim (Transmitter) — VST3 + CLAP"; Types: full custom; Flags: fixed
Name: "receiver";    Description: "Tanghim Receiver — VST3 + CLAP";      Types: full custom; Flags: fixed
Name: "m4l";         Description: "Tanghim M4L Receivers (MPE + Mono PB) for Ableton Live"

[Files]
; Transmitter — system plugin folders
Source: "{#PluginsDir}\Tanghim.vst3\*"; DestDir: "{commoncf64}\VST3\Tanghim.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: transmitter
Source: "{#PluginsDir}\Tanghim.clap";   DestDir: "{commoncf64}\CLAP";             Flags: ignoreversion;                                  Components: transmitter

; Receiver — system plugin folders
Source: "{#PluginsDir}\Tanghim Receiver.vst3\*"; DestDir: "{commoncf64}\VST3\Tanghim Receiver.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: receiver
Source: "{#PluginsDir}\Tanghim Receiver.clap";   DestDir: "{commoncf64}\CLAP";                      Flags: ignoreversion;                                  Components: receiver

; M4L — staged under {commonappdata} (C:\ProgramData), copied to the user
; profile by [Run] below, then the staging dir is removed. ProgramData is
; world-readable (so the runasoriginaluser copies can read it) but is NOT the
; VST3 plugin folder, so the .amxd files never appear there as stray plugins.
Source: "{#PluginsDir}\Tanghim MPE Receiver.amxd";     DestDir: "{commonappdata}\Tanghim\Staging"; Flags: ignoreversion; Components: m4l
Source: "{#PluginsDir}\Tanghim Mono PB Receiver.amxd"; DestDir: "{commonappdata}\Tanghim\Staging"; Flags: ignoreversion; Components: m4l
Source: "{#PluginsDir}\MTS-ESP-Max-Package\*";         DestDir: "{commonappdata}\Tanghim\Staging\MTS-ESP-Max-Package"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: m4l

[Run]
; Copy the staged .amxd files into the invoking (non-admin) user's Ableton
; library and the MTS-ESP-Max-Package into both Max 8 and Max 9 user
; libraries. Also delete the legacy combined device left over from prior
; installs.
;
; runasoriginaluser drops elevation so %USERPROFILE% resolves to the real
; user, not the admin that UAC elevated to. NOTE: Inno's [Run] section does
; NOT check a command's exit code (no abort-on-failure flag exists — see
; jrsoftware.org/ishelp/topic_runsection.htm), so a failed copy here is
; silently swallowed; the "|| exit /b 1" only affects cmd's own return value.
; Proper failure handling would require a [Code] section using Exec/ResultCode.
; This is also why the destination below assumes the default Ableton User
; Library location — if the user relocated it, the copy lands in the wrong
; place without any error. TODO: resolve the real User Library path + add
; [Code] error handling.
;
; The MTS-ESP-Max-Package must live in the Max **Packages** folder:
; %USERPROFILE%\Documents\Max <N>\Packages\MTS-ESP-Max-Package\ — NOT Library\.
; Packages\ is Max's structured-package search path; Library\ is a flat
; catch-all that errors on Windows (it only happened to resolve on macOS).
; A standalone .mxe64 next to the .amxd does NOT work for frozen .amxd loads.
; The stale Library\ copy from prior installs is removed below.
Filename: "{cmd}"; \
  Parameters: "/c mkdir ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"" 2>nul & del /Q ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim Receiver.amxd"" 2>nul & copy /Y ""{commonappdata}\Tanghim\Staging\Tanghim MPE Receiver.amxd"" ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim MPE Receiver.amxd"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing Tanghim MPE Receiver…"

Filename: "{cmd}"; \
  Parameters: "/c copy /Y ""{commonappdata}\Tanghim\Staging\Tanghim Mono PB Receiver.amxd"" ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim Mono PB Receiver.amxd"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing Tanghim Mono PB Receiver…"

Filename: "{cmd}"; \
  Parameters: "/c mkdir ""%USERPROFILE%\Documents\Max 8\Packages"" 2>nul & rmdir /S /Q ""%USERPROFILE%\Documents\Max 8\Packages\MTS-ESP-Max-Package"" 2>nul & rmdir /S /Q ""%USERPROFILE%\Documents\Max 8\Library\MTS-ESP-Max-Package"" 2>nul & xcopy /E /I /Y ""{commonappdata}\Tanghim\Staging\MTS-ESP-Max-Package"" ""%USERPROFILE%\Documents\Max 8\Packages\MTS-ESP-Max-Package"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing MTS-ESP Max Package (Max 8)…"

Filename: "{cmd}"; \
  Parameters: "/c mkdir ""%USERPROFILE%\Documents\Max 9\Packages"" 2>nul & rmdir /S /Q ""%USERPROFILE%\Documents\Max 9\Packages\MTS-ESP-Max-Package"" 2>nul & rmdir /S /Q ""%USERPROFILE%\Documents\Max 9\Library\MTS-ESP-Max-Package"" 2>nul & xcopy /E /I /Y ""{commonappdata}\Tanghim\Staging\MTS-ESP-Max-Package"" ""%USERPROFILE%\Documents\Max 9\Packages\MTS-ESP-Max-Package"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing MTS-ESP Max Package (Max 9)…"

; Remove the ProgramData staging dir now that the copies have consumed it.
; Runs without runasoriginaluser (admin owns ProgramData) and exits 0 so a
; missing dir never affects the install result. Runs last.
Filename: "{cmd}"; \
  Parameters: "/c rmdir /S /Q ""{commonappdata}\Tanghim\Staging"" 2>nul & exit /b 0"; \
  Flags: runhidden waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Cleaning up…"
