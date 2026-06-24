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

; M4L — staged under {app}, copied to user profile by [Run] below
Source: "{#PluginsDir}\Tanghim MPE Receiver.amxd";     DestDir: "{app}\m4l-staging"; Flags: ignoreversion; Components: m4l
Source: "{#PluginsDir}\Tanghim Mono PB Receiver.amxd"; DestDir: "{app}\m4l-staging"; Flags: ignoreversion; Components: m4l

[Run]
; Copy the staged .amxd files into the invoking (non-admin) user's Ableton library
; and delete the legacy combined device left over from prior installs.
; runasoriginaluser drops elevation so %USERPROFILE% resolves to the real user,
; not the admin that UAC elevated to. waituntilterminated + "|| exit /b 1" makes
; a copy failure propagate to Inno Setup instead of being silently swallowed.
Filename: "{cmd}"; \
  Parameters: "/c mkdir ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"" 2>nul & del /Q ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim Receiver.amxd"" 2>nul & copy /Y ""{app}\m4l-staging\Tanghim MPE Receiver.amxd"" ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim MPE Receiver.amxd"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing Tanghim MPE Receiver…"

Filename: "{cmd}"; \
  Parameters: "/c copy /Y ""{app}\m4l-staging\Tanghim Mono PB Receiver.amxd"" ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim Mono PB Receiver.amxd"" || exit /b 1"; \
  Flags: runhidden runasoriginaluser waituntilterminated; \
  Components: m4l; \
  StatusMsg: "Installing Tanghim Mono PB Receiver…"
