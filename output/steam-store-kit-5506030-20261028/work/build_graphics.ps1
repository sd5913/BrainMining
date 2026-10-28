$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$kitRoot=Split-Path $PSScriptRoot -Parent
$sourceDir='C:\Users\abao\Desktop\挖哦素材'
$generated='C:\Users\abao\.codex\generated_images\01a1537f-2b03-7b31-9e60-395ef3da0243'
$fixed='C:\Users\abao\Desktop\UE-Projects\BrainMining\output\imagegen\room-campaign-monitor-fixed-v2'
foreach($dir in @('02_商店图片','03_游戏库图片','04_图标','07_实机截图','08_备用推广图','09_原始LOGO与配置参考')){New-Item -ItemType Directory -Path (Join-Path $kitRoot $dir) -Force|Out-Null}
$logoPath=Join-Path $sourceDir 'logo2.png'
$logo=[System.Drawing.Bitmap]::FromFile($logoPath)
$minX=$logo.Width;$minY=$logo.Height;$maxX=-1;$maxY=-1
for($y=0;$y -lt $logo.Height;$y++){for($x=0;$x -lt $logo.Width;$x++){if($logo.GetPixel($x,$y).A -gt 0){$minX=[math]::Min($minX,$x);$minY=[math]::Min($minY,$y);$maxX=[math]::Max($maxX,$x);$maxY=[math]::Max($maxY,$y)}}}
$logoRect=New-Object System.Drawing.Rectangle($minX,$minY,($maxX-$minX+1),($maxY-$minY+1))
$jobs=@(
 @{File='02_商店图片/header_capsule_920x430.png';Source='exec-13f01ba7-cc2b-4f63-be45-a7bfb31c6ca5.png';W=920;H=430;LW=370;LX=48;LY=60;Slot='Header Capsule'},
 @{File='02_商店图片/small_capsule_462x174.png';Source='exec-13f01ba7-cc2b-4f63-be45-a7bfb31c6ca5.png';W=462;H=174;LW=232;LX=10;LY=4;Slot='Small Capsule'},
 @{File='02_商店图片/main_capsule_1232x706.png';Source='exec-c824d94e-dfce-4b71-8f76-8022f97a32bf.png';W=1232;H=706;LW=400;LX=180;LY=65;Slot='Main Capsule'},
 @{File='02_商店图片/vertical_capsule_748x896.png';Source='exec-3101201b-5841-4cba-8756-6265b29c91f8.png';W=748;H=896;LW=485;LX=131;LY=10;Slot='Vertical Capsule'},
 @{File='03_游戏库图片/library_capsule_600x900.png';Source='exec-6cff89e0-a784-4d91-81b9-f5022d013e0e.png';W=600;H=900;LW=390;LX=105;LY=55;Slot='Library Capsule'},
 @{File='08_备用推广图/event_cover_800x450.png';Source='exec-c824d94e-dfce-4b71-8f76-8022f97a32bf.png';W=800;H=450;LW=258;LX=120;LY=42;Slot='Optional Event Cover'},
 @{File='08_备用推广图/event_header_1920x622.png';Source='exec-13f01ba7-cc2b-4f63-be45-a7bfb31c6ca5.png';W=1920;H=622;LW=750;LX=100;LY=35;Slot='Optional Event Header'}
)
$manifest=New-Object System.Collections.Generic.List[object]
foreach($job in $jobs){
 $source=Join-Path $generated $job.Source
 $img=[System.Drawing.Image]::FromFile($source)
 $canvas=New-Object System.Drawing.Bitmap($job.W,$job.H)
 $g=[System.Drawing.Graphics]::FromImage($canvas)
 $g.CompositingQuality=[System.Drawing.Drawing2D.CompositingQuality]::HighQuality
 $g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
 $cw=[double]$img.Width;$ch=$cw*$job.H/$job.W
 if($ch -gt $img.Height){$ch=[double]$img.Height;$cw=$ch*$job.W/$job.H}
 $cx=($img.Width-$cw)/2;$cy=($img.Height-$ch)/2
 $rect=New-Object System.Drawing.Rectangle(0,0,$job.W,$job.H)
 $attrs=New-Object System.Drawing.Imaging.ImageAttributes
 $attrs.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
 $g.DrawImage($img,$rect,[single]$cx,[single]$cy,[single]$cw,[single]$ch,[System.Drawing.GraphicsUnit]::Pixel,$attrs)
 $lh=[int][math]::Round($job.LW*$logoRect.Height/$logoRect.Width)
 $lr=New-Object System.Drawing.Rectangle($job.LX,$job.LY,$job.LW,$lh)
 $g.DrawImage($logo,$lr,$logoRect.X,$logoRect.Y,$logoRect.Width,$logoRect.Height,[System.Drawing.GraphicsUnit]::Pixel)
 $target=Join-Path $kitRoot $job.File
 $canvas.Save($target,[System.Drawing.Imaging.ImageFormat]::Png)
 $manifest.Add([pscustomobject]@{File=$job.File;Slot=$job.Slot;Width=$job.W;Height=$job.H;Source=$source;LogoSHA256=(Get-FileHash -LiteralPath $logoPath).Hash;Bytes=(Get-Item -LiteralPath $target).Length})
 $attrs.Dispose();$g.Dispose();$canvas.Dispose();$img.Dispose()
}
Copy-Item -LiteralPath (Join-Path $kitRoot '02_商店图片/header_capsule_920x430.png') -Destination (Join-Path $kitRoot '03_游戏库图片/library_header_920x430.png')
$hero=[System.Drawing.Bitmap]::FromFile((Join-Path $sourceDir 'library-background-3840x1240.png'))
$monitorPatch=[System.Drawing.Image]::FromFile((Join-Path $generated 'exec-d638e618-bdc8-486e-a731-afe008424055.png'))
$hg=[System.Drawing.Graphics]::FromImage($hero)
$clip=New-Object System.Drawing.Drawing2D.GraphicsPath
$clip.AddPolygon([System.Drawing.PointF[]]@((New-Object System.Drawing.PointF(2365,558)),(New-Object System.Drawing.PointF(2535,550)),(New-Object System.Drawing.PointF(2535,718)),(New-Object System.Drawing.PointF(2365,728))))
$hg.SetClip($clip);$hg.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;$hg.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$patchRect=New-Object System.Drawing.Rectangle(2300,540,245,190)
$hg.DrawImage($monitorPatch,$patchRect,0,0,$monitorPatch.Width,$monitorPatch.Height,[System.Drawing.GraphicsUnit]::Pixel)
$hg.Dispose();$clip.Dispose();$monitorPatch.Dispose()
$hero.Save((Join-Path $kitRoot '03_游戏库图片/library_hero_3840x1240.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$hero.Dispose()
$logoW=[int][math]::Round(720*$logoRect.Width/$logoRect.Height)
$logoCanvas=New-Object System.Drawing.Bitmap($logoW,720,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g=[System.Drawing.Graphics]::FromImage($logoCanvas);$g.Clear([System.Drawing.Color]::Transparent)
$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$rect=New-Object System.Drawing.Rectangle(0,0,$logoW,720)
$g.DrawImage($logo,$rect,$logoRect.X,$logoRect.Y,$logoRect.Width,$logoRect.Height,[System.Drawing.GraphicsUnit]::Pixel)
$logoCanvas.Save((Join-Path $kitRoot ('03_游戏库图片/library_logo_'+$logoW+'x720.png')),[System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$logoCanvas.Dispose();$logo.Dispose()
Copy-Item -LiteralPath $logoPath -Destination (Join-Path $kitRoot '09_原始LOGO与配置参考/logo2_original_unchanged.png')
Copy-Item -LiteralPath (Join-Path (Get-Location) 'Docs/SteamAchievementDefinitions.csv') -Destination (Join-Path $kitRoot '09_原始LOGO与配置参考/achievements_reference_NOT_store_import.csv')
$icon=[System.Drawing.Image]::FromFile((Join-Path $sourceDir '图标2.png'))
foreach($size in @(256,512,184)){
 $canvas=New-Object System.Drawing.Bitmap($size,$size)
 $g=[System.Drawing.Graphics]::FromImage($canvas);$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
 $g.DrawImage($icon,0,0,$size,$size)
 if($size -eq 184){$path=Join-Path $kitRoot '04_图标/app_icon_184x184.jpg';$canvas.Save($path,[System.Drawing.Imaging.ImageFormat]::Jpeg)}else{$path=Join-Path $kitRoot ('04_图标/shortcut_icon_'+$size+'x'+$size+'.png');$canvas.Save($path,[System.Drawing.Imaging.ImageFormat]::Png)}
 $g.Dispose();$canvas.Dispose()
}
$icon.Dispose()
$page=[System.Drawing.Image]::FromFile((Join-Path $generated 'exec-c824d94e-dfce-4b71-8f76-8022f97a32bf.png'))
$canvas=New-Object System.Drawing.Bitmap(1438,810)
$g=[System.Drawing.Graphics]::FromImage($canvas);$g.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($page,0,0,1438,810)
$brush=New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(90,0,0,0))
$g.FillRectangle($brush,0,0,1438,810)
$canvas.Save((Join-Path $kitRoot '02_商店图片/page_background_1438x810.png'),[System.Drawing.Imaging.ImageFormat]::Png)
$brush.Dispose();$g.Dispose();$canvas.Dispose();$page.Dispose()
$screens=@(
 @('01_mining_gold_2560x1440.png','微信图片_20261021092440_4474_743.png'),
 @('02_mushroom_cavern_2560x1440.png','微信图片_20261021092440_4475_743.png'),
 @('03_office_2560x1440.png','微信图片_20261021092440_4479_743.png'),
 @('04_echo_passage_2560x1440.png','微信图片_20261021092440_4476_743.png'),
 @('05_giant_room_2560x1440.png','微信图片_20261021092440_4477_743.png'),
 @('06_museum_2560x1440.png','微信图片_20261021092440_4480_743.png'),
 @('07_red_cavern_2560x1440.png','微信图片_20261021092440_4478_743.png')
)
foreach($screen in $screens){
 $original=Join-Path $sourceDir $screen[1];$target=Join-Path $kitRoot ('07_实机截图/'+$screen[0])
 Copy-Item -LiteralPath $original -Destination $target
 $manifest.Add([pscustomobject]@{File=('07_实机截图/'+$screen[0]);Slot='Gameplay Screenshot';Width=2560;Height=1440;Source=$original;OriginalSHA256=(Get-FileHash -LiteralPath $original).Hash;Bytes=(Get-Item -LiteralPath $target).Length})
}
$manifest|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $kitRoot 'work/graphics-provenance.json') -Encoding utf8
Get-ChildItem -LiteralPath $kitRoot -Directory|ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -File}|Where-Object {$_.Extension -in @('.png','.jpg')}|Select-Object Name,Length|ConvertTo-Json
