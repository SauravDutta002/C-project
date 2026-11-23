# Generates resources/app.ico (PNG-compressed, 16/32/48/256 px) for RailNet PMS.
# Usage:  powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1
param([string]$Out = "$PSScriptRoot\..\resources\app.ico")

Add-Type -AssemblyName System.Drawing

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-IconPng([int]$s) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $k = $s / 64.0

    $bgRect = New-Object System.Drawing.RectangleF (2 * $k), (2 * $k), (60 * $k), (60 * $k)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $bgRect,
        ([System.Drawing.Color]::FromArgb(255, 59, 130, 246)), ([System.Drawing.Color]::FromArgb(255, 30, 58, 138)), 60.0
    $g.FillPath($bg, (New-RoundedPath (2 * $k) (2 * $k) (60 * $k) (60 * $k) (14 * $k)))

    $white = [System.Drawing.Brushes]::White
    $g.FillPath($white, (New-RoundedPath (17 * $k) (10 * $k) (30 * $k) (34 * $k) (8 * $k)))
    $glass = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 30, 64, 175))
    $g.FillPath($glass, (New-RoundedPath (21 * $k) (15 * $k) (22 * $k) (12 * $k) (3 * $k)))
    $lamp = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 250, 204, 21))
    $g.FillEllipse($lamp, 21.5 * $k, 33 * $k, 6 * $k, 6 * $k)
    $g.FillEllipse($lamp, 36.5 * $k, 33 * $k, 6 * $k, 6 * $k)

    $rail = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ([float](3.2 * $k))
    $rail.StartCap = 'Round'; $rail.EndCap = 'Round'
    $g.DrawLine($rail, 22 * $k, 47 * $k, 14 * $k, 57 * $k)
    $g.DrawLine($rail, 42 * $k, 47 * $k, 50 * $k, 57 * $k)
    $sleeper = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(200, 255, 255, 255)), ([float](2.2 * $k))
    $g.DrawLine($sleeper, 19 * $k, 51 * $k, 45 * $k, 51 * $k)
    $g.DrawLine($sleeper, 16 * $k, 55.5 * $k, 48 * $k, 55.5 * $k)

    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    return , $ms.ToArray()
}

$sizes = 16, 24, 32, 48, 256
$images = @(); foreach ($s in $sizes) { $images += , (New-IconPng $s) }

$fs = [System.IO.File]::Create((Resolve-Path -LiteralPath (Split-Path $Out)).Path + "\" + (Split-Path $Out -Leaf))
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $len = $images[$i].Length
    $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32); $bw.Write([UInt32]$len); $bw.Write([UInt32]$offset)
    $offset += $len
}
foreach ($img in $images) { $bw.Write($img) }
$bw.Close()
Write-Host "Wrote $Out"
