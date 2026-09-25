# docs/NERITH.md -> docs/Nerith.pdf
#
# Markdown je prosty text a PDF musi vyzerat ako dokument k tejto hre, nie ako
# vypis suboru. Prevod je tu vlastny z jedneho dovodu: pandoc ani wkhtmltopdf
# na tomto stroji nie su a pridavat zavislost kvoli jednemu dokumentu je proti
# pravidlam projektu. Chrome je uz nainstalovany a vie tlacit headless.
#
# Podporena je len ta cast markdownu, ktoru dokument naozaj pouziva: nadpisy,
# odstavce, odrazky, cislovane zoznamy, tabulky, **tucne**, `kod`. Nic viac.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $root 'docs\NERITH.md'
$html = Join-Path $root 'docs\Nerith.html'
$pdf  = Join-Path $root 'docs\Nerith.pdf'

function Esc([string]$s)
{
    $s = $s -replace '&', '&amp;' -replace '<', '&lt;' -replace '>', '&gt;'
    return $s
}

# Vnutro riadku: `kod` sa berie prvy, aby sa v nom uz nic dalsie nenahradzalo.
function Inline([string]$s)
{
    $s = Esc $s
    $s = [regex]::Replace($s, '`([^`]+)`', '<code>$1</code>')
    $s = [regex]::Replace($s, '\*\*([^*]+)\*\*', '<strong>$1</strong>')
    $s = $s -replace '\\~', '~' -replace '\\_', '_'
    return $s
}

function Cells([string]$line)
{
    $t = $line.Trim().Trim('|')
    return ($t -split '\|') | ForEach-Object { $_.Trim() }
}

$lines = [IO.File]::ReadAllLines($src)
$out = New-Object System.Text.StringBuilder
$i = 0
$title = ''
$subtitle = ''

while ($i -lt $lines.Count)
{
    $line = $lines[$i]

    if ($line -match '^\s*$') { $i++; continue }

    # Tabulka: hlavicka, oddelovac, riadky.
    if ($line -match '^\s*\|' -and $i + 1 -lt $lines.Count -and $lines[$i + 1] -match '^\s*\|[\s\-:|]+\|\s*$')
    {
        [void]$out.Append('<table><thead><tr>')
        foreach ($c in Cells $line) { [void]$out.Append('<th>' + (Inline $c) + '</th>') }
        [void]$out.Append('</tr></thead><tbody>')
        $i += 2
        while ($i -lt $lines.Count -and $lines[$i] -match '^\s*\|')
        {
            [void]$out.Append('<tr>')
            foreach ($c in Cells $lines[$i]) { [void]$out.Append('<td>' + (Inline $c) + '</td>') }
            [void]$out.Append('</tr>')
            $i++
        }
        [void]$out.Append('</tbody></table>')
        continue
    }

    if ($line -match '^(#{1,4})\s+(.*)$')
    {
        $level = $Matches[1].Length
        $text = Inline $Matches[2]
        if ($level -eq 1 -and $title -eq '') { $title = $Matches[2]; $i++; continue }
        [void]$out.Append("<h$level>$text</h$level>")
        $i++
        continue
    }

    if ($line -match '^\s*-\s+(.*)$')
    {
        [void]$out.Append('<ul>')
        while ($i -lt $lines.Count -and $lines[$i] -match '^\s*-\s+(.*)$')
        {
            [void]$out.Append('<li>' + (Inline $Matches[1]) + '</li>')
            $i++
        }
        [void]$out.Append('</ul>')
        continue
    }

    if ($line -match '^\s*\d+\.\s+(.*)$')
    {
        [void]$out.Append('<ol>')
        while ($i -lt $lines.Count -and $lines[$i] -match '^\s*\d+\.\s+(.*)$')
        {
            [void]$out.Append('<li>' + (Inline $Matches[1]) + '</li>')
            $i++
        }
        [void]$out.Append('</ol>')
        continue
    }

    # Prvy obycajny odstavec je datumovy riadok z dokumentu - ide do hlavicky.
    if ($subtitle -eq '' -and $title -ne '' -and $out.Length -eq 0)
    {
        $subtitle = Inline $line
        $i++
        continue
    }

    [void]$out.Append('<p>' + (Inline $line) + '</p>')
    $i++
}

$body = $out.ToString()
$today = Get-Date -Format 'yyyy-MM-dd'

$page = @"
<!doctype html>
<html lang="sk">
<head>
<meta charset="utf-8">
<title>$title</title>
<style>
  /* Tmava paleta je ta ista, v ktorej sa hra hra: hlboka modroceierna voda,
     studene svetlo lampy. Dokument o podvodnom hororze v bielom liste vyzera
     ako danove priznanie. */
  :root {
    --ink:   #dfe8ef;
    --dim:   #93a6b5;
    --deep:  #080e14;
    --panel: #0e1720;
    --line:  #1d2c39;
    --lamp:  #7fd4e8;
    --warm:  #e8c47a;
  }
  @page { size: A4; margin: 16mm 14mm 18mm 14mm; }
  * { box-sizing: border-box; }
  html { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
  body {
    margin: 0; background: var(--deep); color: var(--ink);
    font: 10.5pt/1.55 "Segoe UI", system-ui, sans-serif;
  }
  .cover {
    height: 247mm; display: flex; flex-direction: column;
    justify-content: center; page-break-after: always;
    background:
      radial-gradient(120% 70% at 50% 8%, rgba(127,212,232,.16), transparent 60%),
      linear-gradient(180deg, #0b131b 0%, #05090d 100%);
    padding: 0 12mm; position: relative;
  }
  .cover h1 {
    font-size: 40pt; letter-spacing: .26em; margin: 0 0 6mm;
    text-transform: uppercase; color: #fff; font-weight: 300;
  }
  .cover .rule { height: 1px; background: linear-gradient(90deg, var(--lamp), transparent); margin-bottom: 6mm; }
  .cover .lead { color: var(--dim); font-size: 12pt; max-width: 120mm; }
  .cover .foot {
    position: absolute; left: 12mm; right: 12mm; bottom: 10mm;
    display: flex; justify-content: space-between;
    color: #5c7186; font-size: 8.5pt; letter-spacing: .14em; text-transform: uppercase;
  }
  h2 {
    font-size: 15pt; font-weight: 500; color: #fff; margin: 9mm 0 3mm;
    padding-bottom: 2mm; border-bottom: 1px solid var(--line);
    letter-spacing: .04em; page-break-after: avoid;
  }
  h3 {
    font-size: 11.5pt; font-weight: 600; color: var(--lamp);
    margin: 6mm 0 2mm; letter-spacing: .03em; page-break-after: avoid;
  }
  p { margin: 0 0 3mm; }
  strong { color: #fff; font-weight: 600; }
  code {
    font-family: "Cascadia Mono", Consolas, monospace; font-size: 9pt;
    background: #132030; color: var(--warm);
    padding: .5mm 1.4mm; border-radius: 2px;
  }
  ul, ol { margin: 0 0 3mm; padding-left: 6mm; }
  li { margin-bottom: 1.4mm; }
  li::marker { color: var(--lamp); }
  table {
    width: 100%; border-collapse: collapse; margin: 0 0 4mm;
    font-size: 9.5pt; page-break-inside: avoid;
  }
  th {
    text-align: left; color: var(--lamp); font-weight: 600;
    font-size: 8.5pt; letter-spacing: .1em; text-transform: uppercase;
    padding: 2mm 2.5mm; border-bottom: 1px solid var(--line);
  }
  td { padding: 1.8mm 2.5mm; border-bottom: 1px solid #16222d; vertical-align: top; }
  tbody tr:nth-child(even) { background: var(--panel); }
</style>
</head>
<body>
  <section class="cover">
    <h1>Nerith</h1>
    <div class="rule"></div>
    <div class="lead">$title</div>
    <div class="foot"><span>Patrik Nottny</span><span>$today</span></div>
  </section>
  $body
</body>
</html>
"@

[IO.File]::WriteAllText($html, $page, (New-Object Text.UTF8Encoding $false))

$chrome = 'C:\Program Files\Google\Chrome\Application\chrome.exe'
if (-not (Test-Path $chrome)) { $chrome = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' }

$uri = ([Uri]$html).AbsoluteUri
& $chrome --headless --disable-gpu --no-pdf-header-footer --print-to-pdf="$pdf" $uri | Out-Null

if (Test-Path $pdf) { Write-Host ("PDF: {0} ({1:N0} B)" -f $pdf, (Get-Item $pdf).Length) }
else { Write-Host "PDF sa nevytvoril"; exit 1 }
