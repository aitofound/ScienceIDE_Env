#!MC 1410
$!ReadStyleSheet  "untitled.sty"
  IncludePlotStyle = Yes
  IncludeText = Yes
  IncludeGeom = Yes
  IncludeAuxData = Yes
  IncludeStreamPositions = Yes
  IncludeContourLevels = Yes
  Merge = No
  IncludeFrameSizeAndPosition = No
$!Pick AddAtPosition
  X = 1.3657086224
  Y = 0.276263627354
  ConsiderStyle = Yes
$!FrameControl ActivateByNumber
  Frame = 1
$!FrameLayout ShowBorder = No
$!FrameLayout IsTransparent = Yes
$!RedrawAll 
$!Pick AddAtPosition
  X = 4.43409316155
  Y = 2.85406342914
  ConsiderStyle = Yes
$!View Fit
$!Pick AddAtPosition
  X = 4.63032705649
  Y = 2.30104063429
  ConsiderStyle = Yes
$!LineMap [6]  Lines{LineThickness = 0.800000000000000044}
$!RedrawAll 
$!AttachGeom 
  GeomType = GeomImage
  PositionCoordSys = Frame
  AnchorPos
    {
    X = 25
    Y = 30.97904624277457
    }
  ImageFileName = 'data.png'
  PixelAspectRatio = 1
  RawData
50 38.0419075145 
$!Pick AddAtPosition
  X = 4.603567889
  Y = 3.52304261645
  ConsiderStyle = Yes
$!Pick Shift
  X = -0.767096134787
  Y = -0.767096134787
  PickSubposition = TopLeft
$!Pick Shift
  X = 0.606541129832
  Y = 0.383548067393
  PickSubposition = BottomRight
$!Pick AddAtPosition
  X = 6.80673934589
  Y = 4.7896432111
  CollectingObjectsMode = HomogeneousAdd
  ConsiderStyle = Yes
$!Pick Clear
$!AttachGeom 
  GeomType = GeomImage
  PositionCoordSys = Frame
  AnchorPos
    {
    X = 16.47670961347869
    Y = 25.85149478393877
    }
  DrawOrder = BeforeData
  ImageFileName = 'data.png'
  PixelAspectRatio = 1
  RawData
65.2626395226 49.6543059373 
$!Pick AddAtPosition
  X = 2.12388503469
  Y = 1.62314172448
  ConsiderStyle = Yes
$!Pick AddAtPosition
  X = 2.16848364718
  Y = 1.5517839445
  ConsiderStyle = Yes
$!XYLineAxis XDetail 1 {RangeMin = 100000000}
$!XYLineAxis XDetail 1 {RangeMax = 100000000000}
$!XYLineAxis YDetail 1 {RangeMin = 200000}
$!XYLineAxis YDetail 1 {RangeMax = 1500000}
$!RedrawAll 
