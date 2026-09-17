  filename='./SC/z=0.outs' 
  npict=1			
  firstpict=60
  read_data		
  window,0		
  func='{br}*r^2<0.2>(-0.2) bx;by ur ux;uy' 	
  !x.range=[-5,5]
  !y.range=[-5,5]
  savemovie='mp4'	
  videorate=3		
  videofile='./SC/z=0_range5'	
  firstpict=60
  animate_data	

  exit
