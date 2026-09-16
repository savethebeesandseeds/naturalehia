[CmdletBinding()]
param([switch] $Render)

# Compile only in the existing project container. Lifecycle belongs to ../container.sh.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$paperDirectory = $PSScriptRoot
$containerName = 'naturalehia-fostering-cellular-agriculture'
$expectedImage = 'emscripten/emsdk:6.0.5@sha256:76a44fff907397784decc435115d07fcb9587a4f1504977f39f3745e538e3a1e'
$outputDirectory = Join-Path $paperDirectory 'build'
$stem = 'fostering-cellular-agriculture-one-page'
$pdfName = "$stem.pdf"

function Invoke-PaperDocker {
    param([Parameter(Mandatory = $true)][string[]] $Arguments)
    & docker --context desktop-linux @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Docker $($Arguments[0]) failed ($LASTEXITCODE)." }
}

$inspectionText = & docker --context desktop-linux container inspect $containerName
if ($LASTEXITCODE -ne 0) {
    throw 'The existing project container is required; this helper never creates one.'
}
$inspection = @($inspectionText | ConvertFrom-Json)[0]
if ($inspection.Config.Image -ne $expectedImage -or
    $inspection.Config.Labels.'org.naturalehia.fostering-cellular-agriculture.managed' -ne 'true') {
    throw 'Unexpected project container configuration; it has been preserved.'
}
if ($inspection.State.Status -ne 'running') {
    throw 'Start Docker Desktop and run bash container.sh up from the project root, then retry.'
}
$containerId = $inspection.Id
$snapshotDirectory = '/tmp/fca-one-page-' + [guid]::NewGuid().ToString('N')
$containerBuild = "$snapshotDirectory-build"
Invoke-PaperDocker -Arguments @('exec', $containerId, 'mkdir', '-p', $snapshotDirectory)
foreach ($sourceName in @("$stem.tex", 'ieee-preamble.tex', 'references.bib', 'build.sh', 'vendor')) {
    Invoke-PaperDocker -Arguments @('cp', (Join-Path $paperDirectory $sourceName), "${containerId}:${snapshotDirectory}/")
}
Invoke-PaperDocker -Arguments @('exec', $containerId, '/bin/bash', "$snapshotDirectory/build.sh", $containerBuild)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Invoke-PaperDocker -Arguments @('cp', "${containerId}:${containerBuild}/.", $outputDirectory)
$logText = Get-Content -LiteralPath (Join-Path $outputDirectory "$stem.log") -Raw
if ($logText -notmatch 'Output written on [\s\S]+?\(1 page,') {
    throw 'The compiler did not confirm exactly one page. Keep editing before release.'
}
if ($logText -match 'Overfull \\[hv]box|There were undefined references|Citation .+ undefined') {
    throw 'Resolve the layout or citation warnings before release.'
}
$pdfInfo = Get-Command pdfinfo -ErrorAction SilentlyContinue
if ($pdfInfo) {
    $info = & $pdfInfo.Source (Join-Path $outputDirectory $pdfName)
    if ($LASTEXITCODE -ne 0 -or ($info -join "`n") -notmatch '(?m)^Pages:\s+1\s*$') {
        throw 'PDF validation did not confirm exactly one page.'
    }
}
if ($Render) {
    $renderer = Get-Command pdftoppm -ErrorAction Stop
    $previewDirectory = Join-Path $outputDirectory 'preview'
    New-Item -ItemType Directory -Path $previewDirectory -Force | Out-Null
    & $renderer.Source -singlefile -r 150 -png (Join-Path $outputDirectory $pdfName) (Join-Path $previewDirectory 'page')
    if ($LASTEXITCODE -ne 0) { throw 'PDF rendering failed.' }
}
Copy-Item -LiteralPath (Join-Path $outputDirectory $pdfName) -Destination (Join-Path $paperDirectory $pdfName)
@(
    "Built at (UTC): $([DateTime]::UtcNow.ToString('o'))"
    "Container: $containerName"
    "Immutable ID: $containerId"
    "Base image: $($inspection.Config.Image)"
    "Container image ID: $($inspection.Image)"
    "Source snapshot: $snapshotDirectory"
    "Build directory: $containerBuild"
    'Format: IEEEtran conference; US Letter; two columns; normal 10-point body.'
) | Set-Content -LiteralPath (Join-Path $outputDirectory 'build-environment.txt') -Encoding utf8
Write-Output "PDF: $(Join-Path $paperDirectory $pdfName)"
