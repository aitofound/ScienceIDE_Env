#!MC 1410
$!GlobalRGB RedChannelVar = 4
$!GlobalRGB GreenChannelVar = 4
$!GlobalRGB BlueChannelVar = 4
$!SetContourVar 
  Var = 4
  ContourGroup = 1
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 5
  ContourGroup = 2
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 6
  ContourGroup = 3
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 7
  ContourGroup = 4
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 8
  ContourGroup = 5
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 9
  ContourGroup = 6
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 10
  ContourGroup = 7
  LevelInitMode = ResetToNice
$!SetContourVar 
  Var = 11
  ContourGroup = 8
  LevelInitMode = ResetToNice
$!SliceLayers Show = Yes
$!SliceAttributes 1  SliceSurface = ZPlanes
$!ExtractSlices 
  Group = 1
  ExtractMode = SingleZone
$!RedrawAll 
$!FieldLayers ShowContour = Yes
$!SetContourVar 
  Var = 7
  ContourGroup = 1
  LevelInitMode = ResetToNice
$!GlobalContour 1  ColorMapName = 'Small Rainbow'
$!RedrawAll 
$!AlterData 
  Equation = '{r}=sqrt(v1*v1+v2*v2+v3*v3)-6052E3'
$!ExtendedCommand 
  CommandProcessorID = 'Extract Precise Line'
  Command = 'XSTART = 0 YSTART = 0 ZSTART = 0 XEND = -8.6e+06 YEND = 0 ZEND = 0 NUMPTS = 500 EXTRACTTHROUGHVOLUME = F EXTRACTTOFILE = T EXTRACTFILENAME = \'x_minus.dat\' '
