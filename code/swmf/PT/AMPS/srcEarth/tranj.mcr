#!MC 1410
$!PlotType = Cartesian3D
$!CreateRectangularZone 
  IMax = 100
  JMax = 100
  KMax = 100
  X1 = -6.2603
  Y1 = -6.183
  Z1 = -6.2603
  X2 = 6.2603
  Y2 = 6.2603
  Z2 = 6.2603
  XVar = 1
  YVar = 2
  ZVar = 3
$!Pick SetMouseMode
  MouseMode = Select
$!AlterData 
  IgnoreDivideByZero = Yes
  Equation = '{r}=sqrt(v1*v1+v2*v2+v3*v3)'
$!FieldLayers ShowMesh = Yes
$!RedrawAll 
$!SetContourVar 
  Var = 4
  ContourGroup = 1
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 1
  LevelInitMode = ResetToNice
$!GlobalRGB RedChannelVar = 6
$!GlobalRGB GreenChannelVar = 4
$!GlobalRGB BlueChannelVar = 4
$!SetContourVar 
  Var = 4
  ContourGroup = 2
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 5
  ContourGroup = 3
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 4
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 5
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 6
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 7
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 8
  LevelInitMode = ResetToNice
$!IsoSurfaceLayers Show = Yes
$!RedrawAll 
