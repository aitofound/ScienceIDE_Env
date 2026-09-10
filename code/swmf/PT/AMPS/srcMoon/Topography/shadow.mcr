#!MC 1410
$!ReadDataSet  '"/nobackupp17/vtenishe/T/AMPS/srcMoon/Topography/shadow-surface-mesh--i=1.dat" '
  ReadDataOption = New
  ResetStyle = No
  VarLoadMode = ByName
  AssignStrandIDs = Yes
  VarNameList = '"X" "Y" "Z" "Surface shadow attribute" "cos(face illumination angle)" "faceat" "nface" "Mesh File ID" "External NormX" "External NormY" "External NormZ"'
$!PrintSetup Palette = Color
$!ExportSetup ExportFormat = JPEG
$!ExportSetup ImageWidth = 713
$!ExportSetup Quality = 100



$!LOOP 179  
$!ReadDataSet  '"/nobackupp17/vtenishe/T/AMPS/srcMoon/Topography/shadow-surface-mesh--i=|LOOP|.dat" '
  ReadDataOption = New
  ResetStyle = No
  VarLoadMode = ByName
  AssignStrandIDs = Yes
  VarNameList = '"X" "Y" "Z" "Surface shadow attribute" "cos(face illumination angle)" "faceat" "nface" "Mesh File ID" "External NormX" "External NormY" "External NormZ"'
$!ExportSetup ExportFName = '/nobackupp17/vtenishe/T/AMPS/srcMoon/Topography/|LOOP|.jpeg'
$!Export
  ExportRegion = AllFrames
$!ENDLOOP


