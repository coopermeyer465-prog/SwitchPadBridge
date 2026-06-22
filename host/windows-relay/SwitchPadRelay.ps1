param(
    [string]$Esp32 = "192.168.0.107",
    [int]$Port = 7777,
    [ValidateRange(60, 500)]
    [int]$PollHz = 250
)

$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)]
public struct XInputGamepad {
    public ushort Buttons;
    public byte LeftTrigger;
    public byte RightTrigger;
    public short LeftX;
    public short LeftY;
    public short RightX;
    public short RightY;
}

[StructLayout(LayoutKind.Sequential)]
public struct XInputState {
    public uint PacketNumber;
    public XInputGamepad Gamepad;
}

public static class XInputNative {
    [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")]
    public static extern uint GetState(uint userIndex, out XInputState state);
}
"@

$Button = @{
    DpadUp = 0x0001; DpadDown = 0x0002; DpadLeft = 0x0004; DpadRight = 0x0008
    Start = 0x0010; Back = 0x0020; LeftThumb = 0x0040; RightThumb = 0x0080
    LeftShoulder = 0x0100; RightShoulder = 0x0200; Guide = 0x0400
    A = 0x1000; B = 0x2000; X = 0x4000; Y = 0x8000
}

function Test-Button([uint16]$Buttons, [int]$Mask) {
    return ($Buttons -band $Mask) -ne 0
}

function Convert-Axis([int16]$Value, [int]$Deadzone, [bool]$Invert) {
    $signed = [int]$Value
    $magnitude = [Math]::Abs($signed)
    if ($magnitude -le $Deadzone) { return 128 }
    $direction = if ($signed -lt 0) { -1 } else { 1 }
    if ($Invert) { $direction = -$direction }
    $normalized = [Math]::Min(1.0, ($magnitude - $Deadzone) / (32767.0 - $Deadzone))
    return [Math]::Max(1, [Math]::Min(255, 128 + $direction * [Math]::Round($normalized * 127)))
}

function Get-Hat([uint16]$Buttons) {
    $up = Test-Button $Buttons $Button.DpadUp
    $down = Test-Button $Buttons $Button.DpadDown
    $left = Test-Button $Buttons $Button.DpadLeft
    $right = Test-Button $Buttons $Button.DpadRight
    if ($up -and $right) { return 1 }
    if ($right -and $down) { return 3 }
    if ($down -and $left) { return 5 }
    if ($left -and $up) { return 7 }
    if ($up) { return 0 }
    if ($right) { return 2 }
    if ($down) { return 4 }
    if ($left) { return 6 }
    return 8
}

function Convert-State([int]$Player, [XInputState]$State) {
    $gamepad = $State.Gamepad
    $buttons = 0

    # Preserve physical positions when translating Xbox labels to Nintendo labels.
    if (Test-Button $gamepad.Buttons $Button.A) { $buttons = $buttons -bor 0x0002 }
    if (Test-Button $gamepad.Buttons $Button.B) { $buttons = $buttons -bor 0x0004 }
    if (Test-Button $gamepad.Buttons $Button.X) { $buttons = $buttons -bor 0x0001 }
    if (Test-Button $gamepad.Buttons $Button.Y) { $buttons = $buttons -bor 0x0008 }
    if (Test-Button $gamepad.Buttons $Button.LeftShoulder) { $buttons = $buttons -bor 0x0010 }
    if (Test-Button $gamepad.Buttons $Button.RightShoulder) { $buttons = $buttons -bor 0x0020 }
    if ($gamepad.LeftTrigger -gt 30) { $buttons = $buttons -bor 0x0040 }
    if ($gamepad.RightTrigger -gt 30) { $buttons = $buttons -bor 0x0080 }
    $back = Test-Button $gamepad.Buttons $Button.Back
    $start = Test-Button $gamepad.Buttons $Button.Start
    if ($back -and $start) {
        $buttons = $buttons -bor 0x1000
    } else {
        if ($back) { $buttons = $buttons -bor 0x0100 }
        if ($start) { $buttons = $buttons -bor 0x0200 }
    }
    if (Test-Button $gamepad.Buttons $Button.LeftThumb) { $buttons = $buttons -bor 0x0400 }
    if (Test-Button $gamepad.Buttons $Button.RightThumb) { $buttons = $buttons -bor 0x0800 }
    if (Test-Button $gamepad.Buttons $Button.Guide) { $buttons = $buttons -bor 0x1000 }

    $hat = Get-Hat $gamepad.Buttons
    $lx = Convert-Axis $gamepad.LeftX 7849 $false
    $ly = Convert-Axis $gamepad.LeftY 7849 $true
    $rx = Convert-Axis $gamepad.RightX 8689 $false
    $ry = Convert-Axis $gamepad.RightY 8689 $true
    return "device=moonlight-p$($Player + 1)&buttons=$buttons&hat=$hat&lx=$lx&ly=$ly&rx=$rx&ry=$ry"
}

function Get-Neutral([int]$Player) {
    return "device=moonlight-p$($Player + 1)&buttons=0&hat=8&lx=128&ly=128&rx=128&ry=128"
}

$udp = [System.Net.Sockets.UdpClient]::new()
$address = [System.Net.Dns]::GetHostAddresses($Esp32) |
    Where-Object AddressFamily -eq ([System.Net.Sockets.AddressFamily]::InterNetwork) |
    Select-Object -First 1
if ($null -eq $address) { throw "Could not resolve an IPv4 address for $Esp32" }
$endpoint = [System.Net.IPEndPoint]::new($address, $Port)
$encoding = [System.Text.Encoding]::ASCII
$lastPayload = @($null, $null, $null, $null)
$connected = @($false, $false, $false, $false)
$lastHeartbeat = [System.Diagnostics.Stopwatch]::StartNew()
$sleepMs = [Math]::Max(1, [Math]::Floor(1000 / $PollHz))

Write-Host "SwitchPad relay -> $($endpoint.Address):$Port at $PollHz Hz"
Write-Host "Waiting for Sunshine XInput controllers. Press Ctrl+C to stop."

try {
    while ($true) {
        $heartbeat = $lastHeartbeat.ElapsedMilliseconds -ge 250
        for ($player = 0; $player -lt 4; $player++) {
            $state = [XInputState]::new()
            $available = [XInputNative]::GetState($player, [ref]$state) -eq 0
            if ($available) {
                $payload = Convert-State $player $state
                if (-not $connected[$player]) {
                    Write-Host "P$($player + 1) connected"
                    $connected[$player] = $true
                }
            } else {
                $payload = Get-Neutral $player
                if ($connected[$player]) {
                    Write-Host "P$($player + 1) disconnected"
                    $connected[$player] = $false
                }
            }

            if ($payload -ne $lastPayload[$player] -or $heartbeat) {
                $bytes = $encoding.GetBytes($payload)
                [void]$udp.Send($bytes, $bytes.Length, $endpoint)
                $lastPayload[$player] = $payload
            }
        }
        if ($heartbeat) { $lastHeartbeat.Restart() }
        [System.Threading.Thread]::Sleep($sleepMs)
    }
} finally {
    for ($player = 0; $player -lt 4; $player++) {
        $bytes = $encoding.GetBytes((Get-Neutral $player))
        [void]$udp.Send($bytes, $bytes.Length, $endpoint)
    }
    $udp.Dispose()
}
