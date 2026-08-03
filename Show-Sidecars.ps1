# Show-Sidecars.ps1 -- AtomicDOM terminal render of the XJSON sidecar store.
#
# Read-only presentation of the host-authoritative /api/sidecars feed. The terminal
# presents the store; it never mutates it (sidecars are candidate/compute-only).
# Mirrors the design of the www station page and `sw frame` -- the terminal half of
# the dual UI. Any terminal (MICRONAUT.ps1, a plain pwsh) can dot-source or run this.
#
#   pwsh -File Show-Sidecars.ps1                 # render once
#   pwsh -File Show-Sidecars.ps1 -Refresh 3      # live, refresh every 3s
[CmdletBinding()]
param(
    [string]$Feed = "http://127.0.0.1:8787/api/sidecars",
    [int]$Refresh = 0
)

function Get-Sidecars {
    param([string]$Feed)
    try {
        $doc = Invoke-RestMethod -Uri $Feed -Method Get -TimeoutSec 8
        if ($doc.store -and $doc.store.sidecars) { return ,@($doc.store.sidecars) }
        if ($doc.sidecars) { return ,@($doc.sidecars) }
        return ,@()
    } catch {
        Write-Host ("feed unavailable ({0}): {1}" -f $Feed, $_.Exception.Message) -ForegroundColor Red
        return $null
    }
}

function Show-SidecarFrame {
    param($List, [string]$Feed)
    $total = @($List).Count
    $avail = @($List | Where-Object { $_.available }).Count
    Write-Host "+================ SIDECAR STORE ================+"
    Write-Host "| route: sidecar://store     backend: terminal  |"
    Write-Host ("| feed:  {0}" -f $Feed)
    Write-Host "+------------------ SIDECARS -------------------+"
    foreach ($s in $List) {
        $nops = if ($s.ops) { @($s.ops).Count } else { 0 }
        $up   = if ($s.available) { "[up]" } else { "[--]" }
        $color = if ($s.available) { "Green" } else { "DarkGray" }
        Write-Host ("| {0,-20}{1,-14}{2,-9}{3}  ops:{4}" -f $s.name, $s.kind, $s.lane, $up, $nops) -ForegroundColor $color
    }
    Write-Host "+-------------------- STATUS -------------------+"
    Write-Host ("| total: {0}    available: {1}    authority: candidate_only" -f $total, $avail)
    Write-Host "+----------------------------------------------+"
}

do {
    if ($Refresh -gt 0) { Clear-Host }
    $list = Get-Sidecars -Feed $Feed
    if ($null -ne $list) { Show-SidecarFrame -List $list -Feed $Feed }
    if ($Refresh -gt 0) { Start-Sleep -Seconds $Refresh }
} while ($Refresh -gt 0)
