# SwitchPad Windows relay

This relay converts up to four Windows XInput controllers into the four real USB controller interfaces exposed by the ESP32-S3 firmware. Sunshine's Windows host creates an XInput controller for each Moonlight gamepad, so no Sunshine fork is required.

## Windows 11 setup

1. Install Sunshine using its Windows installer.
2. In Sunshine's web UI, open **Troubleshooting**, install ViGEmBus, and restart Windows.
3. Leave **Input > Controller** enabled and set **Input > Gamepad** to `x360`.
4. Pair a Moonlight client and start the Desktop stream.
5. Connect the ESP32 and Nintendo Switch, then double-click `Run-SwitchPadRelay.cmd`.

The equivalent PowerShell command is:

```powershell
powershell -ExecutionPolicy Bypass -File .\SwitchPadRelay.ps1
```

The default target is `192.168.0.107:7777`. Override it when needed:

```powershell
.\SwitchPadRelay.ps1 -Esp32 192.168.0.107 -Port 7777 -PollHz 250
```

Each new Moonlight controller should print `P1 connected` through `P4 connected`. The relay sends changed input immediately and a 250 ms heartbeat. If the relay closes or a controller disconnects, it sends a neutral report; the ESP32 also neutralizes stale input after 900 ms.

The relay preserves physical face-button positions: the bottom Xbox A button becomes Switch B, the right Xbox B button becomes Switch A, and similarly for X/Y. Start and Back become Plus and Minus. The Xbox Guide bit is forwarded as Home when the virtual driver exposes it; pressing Back and Start together is the reliable Home fallback.

## Video-independent test

No capture card is needed for the first test. Stream the Windows desktop through Sunshine, connect a controller or enable Moonlight's on-screen controls, and confirm the relay reports a player. The controller should operate the Switch even though Moonlight is temporarily showing the PC desktop rather than Switch video.
