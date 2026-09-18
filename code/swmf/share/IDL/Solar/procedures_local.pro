;+
; NAME:
;       LEGEND
; PURPOSE:
;       Create an annotation legend for a plot.
; EXPLANATION:
;       NOTE: IDL 8.0 contains a LEGEND() function written in IDL.   Both
;       can be used provided that the one found later in one's !PATH is 
;       explicitly compiled in one's startup file.   Alternatively, use 
;       AL_LEGEND which is identical to the current procedure (and delete
;       this procedure to avoid possible conflict with the IDL 8.0 LEGEND()         
;
;       This procedure makes a legend for a plot.  The legend can contain
;       a mixture of symbols, linestyles, Hershey characters (vectorfont),
;       and filled polygons (usersym).  A test procedure, legendtest.pro,
;       shows legend's capabilities.  Placement of the legend is controlled
;       with keywords like /right, /top, and /center or by using a position
;       keyword for exact placement (position=[x,y]) or via mouse (/position).
; CALLING SEQUENCE:
;       LEGEND [,items][,keyword options]
; EXAMPLES:
;       The call:
;               legend,['Plus sign','Asterisk','Period'],psym=[1,2,3]
;         produces:
;               -----------------
;               |               |
;               |  + Plus sign  |
;               |  * Asterisk   |
;               |  . Period     |
;               |               |
;               -----------------
;         Each symbol is drawn with a plots command, so they look OK.
;         Other examples are given in optional output keywords.
;
;       lines = indgen(6)                       ; for line styles
;       items = 'linestyle '+strtrim(lines,2)   ; annotations
;       legend,items,linestyle=lines            ; vertical legend---upper left
;       items = ['Plus sign','Asterisk','Period']
;       sym = [1,2,3]
;       legend,items,psym=sym                   ; ditto except using symbols
;       legend,items,psym=sym,/horizontal       ; horizontal format
;       legend,items,psym=sym,box=0             ; sans border
;       legend,items,psym=sym,delimiter='='     ; embed '=' betw psym & text
;       legend,items,psym=sym,margin=2          ; 2-character margin
;       legend,items,psym=sym,position=[x,y]    ; upper left in data coords
;       legend,items,psym=sym,pos=[x,y],/norm   ; upper left in normal coords
;       legend,items,psym=sym,pos=[x,y],/device ; upper left in device coords
;       legend,items,psym=sym,/position         ; interactive position
;       legend,items,psym=sym,/right            ; at upper right
;       legend,items,psym=sym,/bottom           ; at lower left
;       legend,items,psym=sym,/center           ; approximately near center
;       legend,items,psym=sym,number=2          ; plot two symbols, not one
;       legend,items,/fill,psym=[8,8,8],colors=[10,20,30]; 3 filled squares
; INPUTS:
;       items = text for the items in the legend, a string array.
;               For example, items = ['diamond','asterisk','square'].
;               You can omit items if you don't want any text labels.
; OPTIONAL INPUT KEYWORDS:
;
;       linestyle = array of linestyle numbers  If linestyle[i] < 0, then omit
;               ith symbol or line to allow a multi-line entry.     If 
;               linestyle = -99 then text will be left-justified.  
;       psym = array of plot symbol numbers.  If psym[i] is negative, then a
;               line connects pts for ith item.  If psym[i] = 8, then the
;               procedure usersym is called with vertices define in the
;               keyword usersym.   If psym[i] = 88, then use the previously
;               defined user symbol.    If 11 <= psym[i] <= 46 then David
;               Fanning's function SYMCAT() will be used for additional symbols.
;               (http://www.dfanning.com/programs/symcat.pro).   Note that
;               PSYM=10 (histogram plot mode) is not allowed since it 
;               cannot be used with the PLOTS command.
;       vectorfont = vector-drawn characters for the sym/line column, e.g.,
;               ['!9B!3','!9C!3','!9D!3'] produces an open square, a checkmark,
;               and a partial derivative, which might have accompanying items
;               ['BOX','CHECK','PARTIAL DERIVATIVE'].
;               There is no check that !p.font is set properly, e.g., -1 for
;               X and 0 for PostScript.  This can produce an error, e.g., use
;               !20 with PostScript and !p.font=0, but allows use of Hershey
;               *AND* PostScript fonts together.
;       N. B.: Choose any of linestyle, psym, and/or vectorfont.  If none is
;               present, only the text is output.  If more than one
;               is present, all need the same number of elements, and normal
;               plot behaviour occurs.
;               By default, if psym is positive, you get one point so there is
;               no connecting line.  If vectorfont[i] = '',
;               then plots is called to make a symbol or a line, but if
;               vectorfont[i] is a non-null string, then xyouts is called.
;       /help = flag to print header
;       /horizontal = flag to make the legend horizontal
;       /vertical = flag to make the legend vertical (D=vertical)
;       box = flag to include/omit box around the legend (D=include)
;		  outline_color = color of box outline (D = !P.color)
;       bthick = thickness of the legend box (D = !P.thick)
;       clear = flag to clear the box area before drawing the legend
;       delimiter = embedded character(s) between symbol and text (D=none)
;       colors = array of colors for plot symbols/lines (D=!P.color)
;       font = scalar font graphics keyword (-1,0 or 1) for text
;       textcolors = array of colors for text (D=!P.color)
;       margin = margin around text measured in characters and lines
;       spacing = line spacing (D=bit more than character height)
;       pspacing = psym spacing (D=3 characters) (when number of symbols is
;             greater than 1)
;       charsize = just like !p.charsize for plot labels
;       charthick = just like !p.charthick for plot labels
;       thick = array of line thickness numbers (D = !P.thick), if used, then 
;               linestyle must also be specified
;       position = data coordinates of the /top (D) /left (D) of the legend
;       normal = use normal coordinates for position, not data
;       device = use device coordinates for position, not data
;       number = number of plot symbols to plot or length of line (D=1)
;       usersym = 2-D array of vertices, cf. usersym in IDL manual. 
;             (/USERSYM =square, default is to use existing USERSYM definition)
;       /fill = flag to fill the usersym
;       /left_legend = flag to place legend snug against left side of plot
;                 window (D)
;       /right_legend = flag to place legend snug against right side of plot
;               window.    If /right,pos=[x,y], then x is position of RHS and
;               text runs right-to-left.
;       /top_legend = flag to place legend snug against top of plot window (D)
;       /bottom = flag to place legend snug against bottom of plot window
;               /top,pos=[x,y] and /bottom,pos=[x,y] produce same positions.
;
;       If LINESTYLE, PSYM, VECTORFONT, THICK, COLORS, or TEXTCOLORS are
;       supplied as scalars, then the scalar value is set for every line or
;       symbol in the legend.
; Outputs:
;       legend to current plot device
; OPTIONAL OUTPUT KEYWORDS:
;       corners = 4-element array, like !p.position, of the normalized
;         coords for the box (even if box=0): [llx,lly,urx,ury].
;         Useful for multi-column or multi-line legends, for example,
;         to make a 2-column legend, you might do the following:
;           c1_items = ['diamond','asterisk','square']
;           c1_psym = [4,2,6]
;           c2_items = ['solid','dashed','dotted']
;           c2_line = [0,2,1]
;           legend,c1_items,psym=c1_psym,corners=c1,box=0
;           legend,c2_items,line=c2_line,corners=c2,box=0,pos=[c1[2],c1[3]]
;           c = [c1[0]<c2[0],c1[1]<c2[1],c1[2]>c2[2],c1[3]>c2[3]]
;           plots,[c[0],c[0],c[2],c[2],c[0]],[c[1],c[3],c[3],c[1],c[1]],/norm
;         Useful also to place the legend.  Here's an automatic way to place
;         the legend in the lower right corner.  The difficulty is that the
;         legend's width is unknown until it is plotted.  In this example,
;         the legend is plotted twice: the first time in the upper left, the
;         second time in the lower right.
;           legend,['1','22','333','4444'],linestyle=indgen(4),corners=corners
;                       ; BOGUS LEGEND---FIRST TIME TO REPORT CORNERS
;           xydims = [corners[2]-corners[0],corners[3]-corners[1]]
;                       ; SAVE WIDTH AND HEIGHT
;           chdim=[!d.x_ch_size/float(!d.x_size),!d.y_ch_size/float(!d.y_size)]
;                       ; DIMENSIONS OF ONE CHARACTER IN NORMALIZED COORDS
;           pos = [!x.window[1]-chdim[0]-xydims[0] $
;                       ,!y.window[0]+chdim[1]+xydims[1]]
;                       ; CALCULATE POSITION FOR LOWER RIGHT
;           plot,findgen(10)    ; SIMPLE PLOT; YOU DO WHATEVER YOU WANT HERE.
;           legend,['1','22','333','4444'],linestyle=indgen(4),pos=pos
;                       ; REDO THE LEGEND IN LOWER RIGHT CORNER
;         You can modify the pos calculation to place the legend where you
;         want.  For example to place it in the upper right:
;           pos = [!x.window[1]-chdim[0]-xydims[0],!y.window[1]-xydims[1]]
; Common blocks:
;       none
; Procedure:
;       If keyword help is set, call doc_library to print header.
;       See notes in the code.  Much of the code deals with placement of the
;       legend.  The main problem with placement is not being
;       able to sense the length of a string before it is output.  Some crude
;       approximations are used for centering.
; Restrictions:
;       Here are some things that aren't implemented.
;       - An orientation keyword would allow lines at angles in the legend.
;       - An array of usersyms would be nice---simple change.
;       - An order option to interchange symbols and text might be nice.
;       - Somebody might like double boxes, e.g., with box = 2.
;       - Another feature might be a continuous bar with ticks and text.
;       - There are no guards to avoid writing outside the plot area.
;       - There is no provision for multi-line text, e.g., '1st line!c2nd line'
;         Sensing !c would be easy, but !c isn't implemented for PostScript.
;         A better way might be to simply output the 2nd line as another item
;         but without any accompanying symbol or linestyle.  A flag to omit
;         the symbol and linestyle is linestyle[i] = -1.
;       - There is no ability to make a title line containing any of titles
;         for the legend, for the symbols, or for the text.
; Side Effects:
; Modification history:
;       write, 24-25 Aug 92, F K Knight (knight@ll.mit.edu)
;       allow omission of items or omission of both psym and linestyle, add
;         corners keyword to facilitate multi-column legends, improve place-
;         ment of symbols and text, add guards for unequal size, 26 Aug 92, FKK
;       add linestyle(i)=-1 to suppress a single symbol/line, 27 Aug 92, FKK
;       add keyword vectorfont to allow characters in the sym/line column,
;         28 Aug 92, FKK
;       add /top, /bottom, /left, /right keywords for automatic placement at
;         the four corners of the plot window.  The /right keyword forces
;         right-to-left printing of menu. 18 Jun 93, FKK
;       change default position to data coords and add normal, data, and
;         device keywords, 17 Jan 94, FKK
;       add /center keyword for positioning, but it is not precise because
;         text string lengths cannot be known in advance, 17 Jan 94, FKK
;       add interactive positioning with /position keyword, 17 Jan 94, FKK
;       allow a legend with just text, no plotting symbols.  This helps in
;         simply describing a plot or writing assumptions done, 4 Feb 94, FKK
;       added thick, symsize, and clear keyword Feb 96, W. Landsman HSTX
;               David Seed, HR Wallingford, d.seed@hrwallingford.co.uk
;       allow scalar specification of keywords, Mar 96, W. Landsman HSTX
;       added charthick keyword, June 96, W. Landsman HSTX
;       Made keyword names  left,right,top,bottom,center longer,
;                                 Aug 16, 2000, Kim Tolbert
;       Added ability to have regular text lines in addition to plot legend 
;       lines in legend.  If linestyle is -99 that item is left-justified.
;       Previously, only option for no sym/line was linestyle=-1, but then text
;       was lined up after sym/line column.    10 Oct 2000, Kim Tolbert
;       Make default value of thick = !P.thick  W. Landsman  Jan. 2001
;       Don't overwrite existing USERSYM definition  W. Landsman Mar. 2002
;	     Added outline_color BT 24 MAY 2004
;       Pass font keyword to xyouts commands.  M. Fitzgerald, Sep. 2005
;       Default spacing, pspacing should be relative to charsize. M. Perrin, July 2007
;       Don't modify position keyword  A. Kimball/ W. Landsman Jul 2007
;       Small update to Jul 2007 for /NORMAL coords.  W. Landsman Aug 2007
;       Use SYMCAT() plotting symbols for 11<=PSYM<=46   W. Landsman  Nov 2009
;       Make a sharper box edge T. Robishaw/W.Landsman July 2010
;       Added BTHICK keyword W. Landsman October 2010
;-
pro legend, items, BOTTOM_LEGEND=bottom, BOX = box, CENTER_LEGEND=center, $
    CHARTHICK=charthick, CHARSIZE = charsize, CLEAR = clear, COLORS = colorsi, $
    CORNERS = corners, DATA=data, DELIMITER=delimiter, DEVICE=device, $
    FILL=fill, HELP = help, HORIZONTAL=horizontal,LEFT_LEGEND=left, $
    LINESTYLE=linestylei, MARGIN=margin, NORMAL=normal, NUMBER=number, $
    POSITION=position,PSPACING=pspacing, PSYM=psymi, RIGHT_LEGEND=right, $
    SPACING=spacing, SYMSIZE=symsize, TEXTCOLORS=textcolorsi, THICK=thicki, $
    TOP_LEGEND=top, USERSYM=usersym,  VECTORFONT=vectorfonti, VERTICAL=vertical, $
    OUTLINE_COLOR = outline_color, FONT = font, BTHICK = bthick
;
;       =====>> HELP
;
compile_opt idl2
on_error,2
if keyword_set(help) then begin & doc_library,'legend' & return & endif
;
;       =====>> SET DEFAULTS FOR SYMBOLS, LINESTYLES, AND ITEMS.
;
 ni = n_elements(items)
 np = n_elements(psymi)
 nl = n_elements(linestylei)
 nth = n_elements(thicki)
 nv = n_elements(vectorfonti)
 nlpv = max([np,nl,nv])
 n = max([ni,np,nl,nv])                                  ; NUMBER OF ENTRIES
strn = strtrim(n,2)                                     ; FOR ERROR MESSAGES
if n eq 0 then message,'No inputs!  For help, type legend,/help.'
if ni eq 0 then begin
  items = replicate('',n)                               ; DEFAULT BLANK ARRAY
endif else begin
  if size(items,/TNAME) NE 'STRING' then message, $
      'First parameter must be a string array.  For help, type legend,/help.'
  if ni ne n then message,'Must have number of items equal to '+strn
endelse
symline = (np ne 0) or (nl ne 0)                        ; FLAG TO PLOT SYM/LINE
 if (np ne 0) and (np ne n) and (np NE 1) then message, $
        'Must have 0, 1 or '+strn+' elements in PSYM array.'
 if (nl ne 0) and (nl ne n) and (nl NE 1) then message, $
         'Must have 0, 1 or '+strn+' elements in LINESTYLE array.'
 if (nth ne 0) and (nth ne n) and (nth NE 1) then message, $
         'Must have 0, 1 or '+strn+' elements in THICK array.'

 case nl of 
 0: linestyle = intarr(n)              ;Default = solid
 1: linestyle = intarr(n)  + linestylei
 else: linestyle = linestylei
 endcase 
 
 case nth of 
 0: thick = replicate(!p.thick,n)      ;Default = !P.THICK
 1: thick = intarr(n) + thicki
 else: thick = thicki
 endcase 

 case np of             ;Get symbols
 0: psym = intarr(n)    ;Default = solid
 1: psym = intarr(n) + psymi
 else: psym = psymi
 endcase 

 case nv of 
 0: vectorfont = replicate('',n)
 1: vectorfont = replicate(vectorfonti,n)
 else: vectorfont = vectorfonti
 endcase 
;
;       =====>> CHOOSE VERTICAL OR HORIZONTAL ORIENTATION.
;
if n_elements(horizontal) eq 0 then begin               ; D=VERTICAL
  if n_elements(vertical) eq 0 then vertical = 1
endif else begin
  if n_elements(vertical) eq 0 then vertical = not horizontal
endelse
;
;       =====>> SET DEFAULTS FOR OTHER OPTIONS.
;
if n_elements(box) eq 0 then box = 1
if n_elements(clear) eq 0 then clear = 0

if n_elements(margin) eq 0 then margin = 0.5
if n_elements(delimiter) eq 0 then delimiter = ''
if n_elements(charsize) eq 0 then charsize = !p.charsize
if n_elements(charthick) eq 0 then charthick = !p.charthick
if charsize eq 0 then charsize = 1
if (n_elements (symsize) eq 0) then symsize= charsize + intarr(n)
if n_elements(number) eq 0 then number = 1
 case N_elements(colorsi) of 
 0: colors = replicate(!P.color,n)     ;Default is !P.COLOR
 1: colors = replicate(colorsi,n)
 else: colors = colorsi
 endcase 

 case N_elements(textcolorsi) of 
 0: textcolors = replicate(!P.color,n)      ;Default is !P.COLOR
 1: textcolors = replicate(textcolorsi,n)
 else: textcolors = textcolorsi
 endcase 
 fill = keyword_set(fill)
if n_elements(usersym) eq 1 then usersym = 2*[[0,0],[0,1],[1,1],[1,0],[0,0]]-1

if n_elements(outline_color) EQ 0 then outline_color = !P.Color

;
;       =====>> INITIALIZE SPACING
;
if n_elements(spacing) eq 0 then spacing = 1.2*charsize
if n_elements(pspacing) eq 0 then pspacing = 3*charsize
xspacing = !d.x_ch_size/float(!d.x_size) * (spacing > charsize)
yspacing = !d.y_ch_size/float(!d.y_size) * (spacing > charsize)
ltor = 1                                        ; flag for left-to-right
if n_elements(left) eq 1 then ltor = left eq 1
if n_elements(right) eq 1 then ltor = right ne 1
ttob = 1                                        ; flag for top-to-bottom
if n_elements(top) eq 1 then ttob = top eq 1
if n_elements(bottom) eq 1 then ttob = bottom ne 1
xalign = ltor ne 1                              ; x alignment: 1 or 0
yalign = -0.5*ttob + 1                          ; y alignment: 0.5 or 1
xsign = 2*ltor - 1                              ; xspacing direction: 1 or -1
ysign = 2*ttob - 1                              ; yspacing direction: 1 or -1
if not ttob then yspacing = -yspacing
if not ltor then xspacing = -xspacing
;
;       =====>> INITIALIZE POSITIONS: FIRST CALCULATE X OFFSET FOR TEXT
;
xt = 0
if nlpv gt 0 then begin                         ; SKIP IF TEXT ITEMS ONLY.
if vertical then begin                          ; CALC OFFSET FOR TEXT START
  for i = 0,n-1 do begin
    if (psym[i] eq 0) and (vectorfont[i] eq '') then num = (number + 1) > 3 else num = number
    if psym[i] lt 0 then num = number > 2       ; TO SHOW CONNECTING LINE
    if psym[i] eq 0 then expand = 1 else expand = 2
    thisxt = (expand*pspacing*(num-1)*xspacing)
    if ltor then xt = thisxt > xt else xt = thisxt < xt
    endfor
endif   ; NOW xt IS AN X OFFSET TO ALIGN ALL TEXT ENTRIES.
endif
;
;       =====>> INITIALIZE POSITIONS: SECOND LOCATE BORDER
;
if !x.window[0] eq !x.window[1] then begin
  plot,/nodata,xstyle=4,ystyle=4,[0],/noerase
endif
;       next line takes care of weirdness with small windows
pos = [min(!x.window),min(!y.window),max(!x.window),max(!y.window)]
case n_elements(position) of
 0: begin
  if ltor then px = pos[0] else px = pos[2]
  if ttob then py = pos[3] else py = pos[1]
  if keyword_set(center) then begin
    if not keyword_set(right) and not keyword_set(left) then $
      px = (pos[0] + pos[2])/2. - xt
    if not keyword_set(top) and not keyword_set(bottom) then $
      py = (pos[1] + pos[3])/2. + n*yspacing
    endif
  nposition = [px,py] + [xspacing,-yspacing]
  end
 1: begin       ; interactive
  message,/inform,'Place mouse at upper left corner and click any mouse button.'
  cursor,x,y,/normal
  nposition = [x,y]
  end
 2: begin       ; convert upper left corner to normal coordinates
  if keyword_set(data) then $
    nposition = convert_coord(position,/to_norm) $
  else if keyword_set(device) then $
    nposition = convert_coord(position,/to_norm,/device) $
  else if not keyword_set(normal) then $
    nposition = convert_coord(position,/to_norm) else nposition= position
  end
 else: message,'Position keyword can have 0, 1, or 2 elements only. Try legend,/help.'
endcase

yoff = 0.25*yspacing*ysign                      ; VERT. OFFSET FOR SYM/LINE.

x0 = nposition[0] + (margin)*xspacing            ; INITIAL X & Y POSITIONS
y0 = nposition[1] - margin*yspacing + yalign*yspacing    ; WELL, THIS WORKS!
;
;       =====>> OUTPUT TEXT FOR LEGEND, ITEM BY ITEM.
;       =====>> FOR EACH ITEM, PLACE SYM/LINE, THEN DELIMITER,
;       =====>> THEN TEXT---UPDATING X & Y POSITIONS EACH TIME.
;       =====>> THERE ARE A NUMBER OF EXCEPTIONS DONE WITH IF STATEMENTS.
;
for iclr = 0,clear do begin
  y = y0                                                ; STARTING X & Y POSITIONS
  x = x0
  if ltor then xend = 0 else xend = 1           ; SAVED WIDTH FOR DRAWING BOX

 if ttob then ii = [0,n-1,1] else ii = [n-1,0,-1]
 for i = ii[0],ii[1],ii[2] do begin
  if vertical then x = x0 else y = y0           ; RESET EITHER X OR Y
  x = x + xspacing                              ; UPDATE X & Y POSITIONS
  y = y - yspacing
  if nlpv eq 0 then goto,TEXT_ONLY              ; FLAG FOR TEXT ONLY
  if (psym[i] eq 0) and (vectorfont[i] eq '') then num = (number + 1) > 3 else num = number
  if psym[i] lt 0 then num = number > 2         ; TO SHOW CONNECTING LINE
  if psym[i] eq 0 then expand = 1 else expand = 2
  xp = x + expand*pspacing*indgen(num)*xspacing
  if (psym[i] gt 0) and (num eq 1) and vertical then xp = x + xt/2.
  yp = y + intarr(num)
  if vectorfont[i] eq '' then yp = yp + yoff
  if psym[i] eq 0 then begin
    xp = [min(xp),max(xp)]                      ; TO EXPOSE LINESTYLES
    yp = [min(yp),max(yp)]                      ; DITTO
    endif
  if (psym[i] eq 8) and (N_elements(usersym) GT 1) then $
                usersym,usersym,fill=fill,color=colors[i]
;; extra by djseed .. psym=88 means use the already defined usersymbol
 if psym[i] eq 88 then p_sym =8 else $
 if psym[i] EQ 10 then $
         message,'PSYM=10 (histogram mode) not allowed to legend.pro' $
 else  if psym[i] GT 8 then p_sym = symcat(psym[i]) else p_sym= psym[i]

  if vectorfont[i] ne '' then begin
;    if (num eq 1) and vertical then xp = x + xt/2      ; IF 1, CENTERED.
    xyouts,xp,yp,vectorfont[i],width=width,color=colors[i] $
      ,size=charsize,align=xalign,charthick = charthick,/norm,font=font
    xt = xt > width
    xp = xp + width/2.
  endif else begin
    if symline and (linestyle[i] ge 0) then plots,xp,yp,color=colors[i] $
      ,/normal,linestyle=linestyle[i],psym=p_sym,symsize=symsize[i], $
      thick=thick[i]
  endelse

  if vertical then x = x + xt else if ltor then x = max(xp) else x = min(xp)
  if symline then x = x + xspacing
  TEXT_ONLY:
  if vertical and (vectorfont[i] eq '') and symline and (linestyle[i] eq -99) then x=x0 + xspacing
  xyouts,x,y,delimiter,width=width,/norm,color=textcolors[i], $
         size=charsize,align=xalign,charthick = charthick,font=font
  x = x + width*xsign
  if width ne 0 then x = x + 0.5*xspacing
  xyouts,x,y,items[i],width=width,/norm,color=textcolors[i],size=charsize, $
             align=xalign,charthick=charthick,font=font
  x = x + width*xsign
  if not vertical and (i lt (n-1)) then x = x+2*xspacing; ADD INTER-ITEM SPACE
  xfinal = (x + xspacing*margin)
  if ltor then xend = xfinal > xend else xend = xfinal < xend   ; UPDATE END X
 endfor

 if (iclr lt clear ) then begin
;       =====>> CLEAR AREA
        x = nposition[0]
        y = nposition[1]
        if vertical then bottom = n else bottom = 1
        ywidth = - (2*margin+bottom-0.5)*yspacing
        corners = [x,y+ywidth,xend,y]
        polyfill,[x,xend,xend,x,x],y + [0,0,ywidth,ywidth,0],/norm,color=-1
;       plots,[x,xend,xend,x,x],y + [0,0,ywidth,ywidth,0],thick=2
 endif else begin

;
;       =====>> OUTPUT BORDER
;
        x = nposition[0]
        y = nposition[1]
        if vertical then bottom = n else bottom = 1
        ywidth = - (2*margin+bottom-0.5)*yspacing
        corners = [x,y+ywidth,xend,y]
        if box then plots,[x,xend,xend,x,x,xend],y + [0,0,ywidth,ywidth,0,0],/norm, $
        	color = outline_color,thick = bthick
        return
 endelse
endfor

end

;--------------------------------------------------------------------

pro cal_pos, nx, ny, x1=x1,x2=x2,y1=y1,y2=y2,y_x_ratio=y_x_ratio,$ 
   lm=lm, rm=rm, bm=bm,tm=tm,dx=dx, xx=xx,dy=dy,yy=yy

if n_elements(lm) ne 1 then lm=0.3
if n_elements(rm) ne 1 then rm=0.25
if n_elements(bm) ne 1 then bm=0.2
if n_elements(tm) ne 1 then tm=0.4

if n_elements(dx) ne 1 then dx=0.3
if n_elements(xx) ne 1 then xx=2.5
if n_elements(dy) ne 1 then dy=0.3
if n_elements(yy) ne 1 then yy=2.5

xtotal=lm+nx*xx+(nx-1)*dx+rm & ytotal=bm+ny*yy+(ny-1)*dy+tm
x1=(lm+indgen(nx)*(xx+dx))/xtotal
x2=(lm+indgen(nx)*(xx+dx)+xx)/xtotal
y1=(bm+indgen(ny)*(yy+dy))/ytotal
y2=(bm+indgen(ny)*(yy+dy)+yy)/ytotal
y_x_ratio =ytotal/xtotal

end

;--------------------------------------------------------------------

pro string_to_array,s,a,n,sep,arraysyntax=arraysyntax, wildcard=wildcard
  ;; copy from the BATSRUS IDL routine, procedures.pro

  if keyword_set(wildcard) and stregex(s, '[?*[]', /boolean) then begin
     spawn,'/bin/ls '+s, a
     n = n_elements(a)
     return
  endif

  if n_elements(s) gt 1 then     $
     a0 = s                     $
  else if keyword_set(sep) then  $
     a0=strsplit(s,sep,/extract) $
  else                           $
     a0=strsplit(s,/extract)

  n0=n_elements(a0)

  if keyword_set(arraysyntax) then begin
     a1 = strarr(300)
     n1 = 0
     for i0 = 0, n0-1 do begin

        s1 = a0(i0)

        a1(n1) = s1
        n1 = n1 + 1

        if strpos(s1,')') eq strlen(s1)-1 then begin
           s1 = strmid(s1,0,strlen(s1)-1)
           s1 = strsplit(s1,'(',/extract)
           if n_elements(s1) eq 2 then begin
              s2 = s1(1)
              s1 = s1(0)

              s2 = strsplit(s2,':',/extract)

              imin   = 1
              stride = 1

              npart  = n_elements(s2)
              iimax = npart-1 < 1

              imax  = fix(s2(iimax))

              if npart gt 1 then imin   = fix(s2(0))

              if npart gt 2 then stride = fix(s2(2))

              width   = string(strlen(s2(iimax)),format='(i1)')
              formstr = "(a,i"+width+"."+width+")"

              n1 = n1 - 1
              for i = imin,imax,stride do begin
                 a1(n1) = string(s1,i,format=formstr)
                 n1 = n1 + 1
              endfor
           endif
        endif
     endfor

     a0 = a1(0:n1-1)
     n0 = n1
  endif

  if not keyword_set(n) then begin
     a=a0
     n=n0
  endif else if n ge n0 then begin
     a=strarr(n)
     a(0:n0-1)=a0
     if n0 lt n then a(n0:n-1)=a0(n0-1)
  endif else begin
     a=strarr(n)
     a=a0(0:n-1)
     print,'Warning: more than',n,' values defined by string: ',s,$
           FORMAT='(a,i3,a,a)'
  endelse

end

;--------------------------------------------------------------------

pro determine_NameSat, TimeEvent, ObsXyz, NameSat, file_sim=file_sim

  NameSat = 'none'

  StaXyz   = get_stereo_coord(TimeEvent,'A')
  StbXyz   = get_stereo_coord(TimeEvent,'B')
  EarthXyz = get_stereo_coord(TimeEvent,'Earth')

  StaXyz   = StaXyz[0:2]/6.957e5
  StbXyz   = StbXyz[0:2]/6.957e5
  EarthXyz = EarthXyz[0:2]/6.957e5

  ;; within 0.1 AU (2.15 Rs ~ 0.1 AU)
  if (total((ObsXyz-StaXyz)^2)   le 3.0) then NameSat='sta'
  if (total((ObsXyz-StbXyz)^2)   le 3.0) then NameSat='stb'
  if (total((ObsXyz-EarthXyz)^2) le 3.0) then NameSat='earth'

  if (NameSat eq 'none') then begin
     print, 'Could not determine Sat for ', file_sim
     print, 'Time     =', TimeEvent
     print, 'ObsXyz   =', ObsXyz
     print, 'StaXyz   =', StaXyz
     print, 'StbXyz   =', StbXyz
     print, 'EarthXyz =', EarthXyz
     exit
  endif
end

;--------------------------------------------------------------------

pro get_euv_info, NameSat = NameSat, SourceName = SourceName,   $
                  InstName = InstName, DetName = DetName,       $
                  titlePlt = titlePlt, titleModel = titleModel, $
                  InstPlt = InstPlt

  SourceName = ''
  InstName   = ''
  DetName    = ''
  titlePlt   = ''
  titleModel = ''
  InstPlt    = ''

  if (NameSat eq 'earth') then begin
     SourceName = ''
     InstName   = 'eit'
     DetName    = ''
     titlePlt   = 'EIT'
     titleModel = 'Model EIT'
     InstPlt    = 'EIT'
  endif

  if (NameSat eq 'sta') then begin
     SourceName = 'STEREO_A'
     InstName   = ''
     DetName    = 'euvi'
     titlePlt   = 'Stereo A'
     titleModel = 'Model Stereo A'
     InstPlt    = 'euvia'
  endif

  if (NameSat eq 'stb') then begin
     SourceName = 'STEREO_B'
     InstName   = ''
     DetName    = 'euvi'
     titlePlt   = 'Stereo B'
     titleModel = 'Model Stereo B'
     InstPlt    = 'euvib'
  endif

end

;--------------------------------------------------------------------

pro get_wl_info, NameSat,SourceName=SourceName, InstName = InstName, $
                 DetName = DetName, titlePlt = titlePlt,             $
                 titleModel = titleModel, InstPlt = InstPlt

  ;; set up source info for download...
  SourceName = ''
  InstName   = ''
  DetName    = ''
  titlePlt   = ''
  titleModel = ''
  InstPlt    = ''

  if (NameSat eq 'earth') then begin
     SourceName = 'SOHO'
     InstName   = 'LASCO'
     if (rMaxSim le 10) then begin
        DetName = 'C2'
        InstPlt = 'C2'
        titlePlt   = 'LASCO C2'
        titleModel = 'Model C2'
     endif else begin
        DetName = 'C3'
        InstPlt = 'C3'
        titlePlt   = 'LASCO C3'
        titleModel = 'Model C3'
     endelse
  endif

  if (NameSat eq 'sta') then begin
     SourceName = 'STEREO_A'
     InstName   = 'SECCHI'
     if (rMaxSim le 10.0) then begin
        DetName = 'COR1'
        InstPlt = 'sta_cor1'
        titlePlt   = 'Stereo A COR1'
        titleModel = 'Model COR1'
     endif else begin
        DetName = 'COR2'
        InstPlt = 'sta_cor2'
        titlePlt   = 'Stereo A COR2'
        titleModel = 'Model COR2'
     endelse
  endif

  if (NameSat eq 'stb') then begin
     SourceName = 'STEREO_B'
     InstName   = 'SECCHI'
     if (rMaxSim le 10.0) then begin
        DetName = 'COR1'
        InstPlt = 'stb_cor1'
        titlePlt   = 'Stereo B COR1'
        titleModel = 'Model COR1'
     endif else begin
        DetName = 'COR2'
        InstPlt = 'stb_cor2'
        titlePlt= 'Stereo B COR2'
        titleModel = 'Model COR2'
     endelse
  endif

end

;--------------------------------------------------------------------

pro set_FilePlotName, TimeEvent, UseTimePlotName = UseTimePlotName,   $
                      TimeStrFile = TimeStrFile, fileplot = fileplot, $
                      InstPlt = InstPlt, dir_plot = dir_plot

  if (not keyword_set(InstPlt))  then InstPlt = ''
  if (not keyword_set(dir_plot)) then dir_plot = '../output/'

  ;; set string to be used in the filename for plotting 
  TimeStrFile = utc2str(anytim2utc(TimeEvent))
  TimeStrFile = strmid(TimeStrFile,0,strpos(TimeStrFile,'.'))

  if (UseTimePlotName) then begin
     ;; so the format is in 'YYYY-MM-DDTHH:MM:SEC.MSEC'
     fileplot = dir_plot + '/'+TimeStrFile+'_los_'+InstPlt
  endif else begin
     CR_number = fix(tim2carr(TimeEvent,/dc))
     fileplot  = dir_plot+'/CR'+string(CR_number,format='(i4)')+'_los_'+InstPlt
  endelse

end

;--------------------------------------------------------------------

pro make_map_swmf_data, dataSim=dataSim, varnames=varnames, namevar=namevar, $
                        nx=nx, ny=ny, dxy=dxy, TimeEvent=TimeEvent,          $
                        map_out = map_out

  xx = dataSim(*,*,0)
  yy = dataSim(*,*,1)

  map_out = fltarr(nx,ny)

  index = where(strmatch(varnames, '*'+namevar+'*', /fold_case), count)
  if count eq 1 then data_local  = dataSim(*,*,index)

  map_out = make_map(data_local, dx=dxy, dy=dxy, time=TimeEvent)

end

;--------------------------------------------------------------------

pro process_aia, filename, aia_map = aia_map, xy_map = xs_map, $
                 ys_map = ys_map, index = index

  if (filename ne '') then begin
     aia_prep, filename, -1, index, data,/normalize,/no_mpo_update
     index2map, index, data, aia_map
     aia_map = rebin_map(aia_map, xs_map, ys_map)
  endif else begin
     aia_map = make_map(fltarr(xs_map, ys_map))
  endelse
  index = index
end

;--------------------------------------------------------------------

pro process_euv, filename, DetName = DetName, InstName = InstName, $
                 euv_map = euv_map, xy_map = xs_map, ys_map = ys_map, $
                 index=index

  if (filename ne '') then begin
     if (DetName eq 'euvi') then begin
        secchi_prep, filename, index, data, /rotate_on
     endif
     if (InstName eq 'eit') then begin
        eit_prep, filename, index, data, /surround
     endif

     index2map, index, data, euv_map
     euv_map = rebin_map(euv_map, xs_map, ys_map)
  endif else begin
     euv_map = make_map(fltarr(xs_map, ys_map))
  endelse
end

;--------------------------------------------------------------------

pro plot_map_local, map, position=position, xrange=xrange, yrange=yrange,  $
                    dmin=dmin, dmax=dmax, charsize=charsize,               $
                    title=title, ytitle=ytitle, xtitle=xtitle

  if (max(map.data) gt 0) then begin
     plot_map, map, position=position, xrange=xrange,yrange=yrange,      $
               dmin=dmin, dmax=dmax, charsize=charsize,              $
               title=title, ytitle=ytitle, xtitle=xtitle, /iso, /log
  endif else begin
     map.data = map.data + 1
     plot_map, map, position=position, xrange=xrange,yrange=yrange,      $
               dmin=dmin, dmax=dmax, charsize=charsize,                $
               title=title, ytitle=ytitle, xtitle=xtitle, /iso, /log, /no_data
     xyouts, 0, 0, 'missing observation', charsize = 0.8*charsize,       $
             /data, alignment=0.5
  endelse
end

;--------------------------------------------------------------------
pro read_swmf_remote_idl, file, TimeEvent=TimeEvent, ObsXyz=ObsXyz, $
                          varnames=varnames, nvars=nvars, data=data,    $
                          TimeSimulationSI = TimeSimulationSI,$
                          nx = nx0, ny = ny0, rMaxSim=rMaxSim,            $
                          DoWl =DoWl, DoPb = DoPb, DoAIA=DoAIA,         $
                          DoXRT=DoXRT, DoEUV=DoEUV
  
  common getpict_param, filename
  common file_head
  common plot_data, grid, x, w
  filename= file
  read_data

  TimeEvent    = ''
  NameLosTable = ''
  ObsXyz   = 0
  nvars    = 0
  data     = 0
  varnames = ''
  
  if (ndim ne 2) then print,'Data should be 2D'
  if (neqpar ne 3) then print,'Obs position should have 3 values'
  ObsXyz = eqpar
  ; Size of the data array
  nx0 = nx[0]
  ny0 = nx[1]
  
  DoWl  = 0
  DoPb  = 0
  DoAIA = 0
  DoEUV = 0
  DoXRT = 0

  lineTmp = headline
  lineTmp=strsplit(lineTmp,' ',/EXTRACT)

  ;; check if it starts with LOS Integrals:
  if (lineTmp(0) EQ 'LOS') then begin
     print,'Reading LOS IDL output header '
  endif else print,'Incorrect output file'

  for i=0, n_elements(lineTmp)-1 do begin
     if (strpos(lineTmp(i), 'TIMEEVENT=') ge 0) then begin
        ii = strpos(lineTmp(i),'=')
        lineTmp0 = strmid(lineTmp(i),ii+1)
        TimeEvent = lineTmp0
     endif
     if (strpos(lineTmp(i), 'TIMEEVENTSTART=') ge 0) then begin
        ii = strpos(lineTmp(i),'=')
        lineTmp0 = strmid(lineTmp(i),ii+1)
        TimeEventStart = lineTmp0
     endif
     if (strpos(lineTmp(i), 'NAMELOSTABLE=') ge 0) then begin
        ii = strpos(lineTmp(i),'=')
        lineTmp0 = strmid(lineTmp(i),ii+1)
        NameLosTable = strlowcase(lineTmp0)
     endif
  endfor
     
  ;; The table name should be entered correctly!!!                     
  if (strpos(NameLosTable,'euvi') ge 0) then DoEUV = 1
  if (strpos(NameLosTable,'aia')  ge 0) then DoAIA = 1
  if (strpos(NameLosTable,'xrt')  ge 0) then DoXRT = 1
  if (strpos(NameLosTable,'eit')  ge 0) then DoEUV = 1
  
  TimeSimulationSI = utc2tai(TimeEvent) - utc2tai(TimeEventStart)
  
  xx=x(*,*,0)
  yy=x(*,*,1)
  rr = sqrt(xx^2+yy^2)
  rMaxSim = max(rr)

  ;;Variable Names and Data
  nvars = n_elements(wnames)
  varnames = strarr(1000)
  varnames = wnames
  data = w
end
;--------------------------------------------------------------------
pro read_swmf_remote_tec, filename, TimeEvent=TimeEvent, ObsXyz=ObsXyz, $
                          varnames=varnames, nvars=nvars, data=data,    $
                          TimeSimulationSI = TimeSimulationSI,          $
                          nx = nx, ny = ny, rMaxSim=rMaxSim,            $
                          DoWl =DoWl, DoPb = DoPb, DoAIA=DoAIA,         $
                          DoXRT=DoXRT, DoEUV=DoEUV

  itype = size(filename,/type)

  if itype eq 7 then begin
     unit  = 0
     found = 0
     while not found do begin
        unit = unit + 1
        stat = fstat(unit)
        if not stat.open then found = 1
     endwhile
     openr,unit,filename
  endif else begin
     print,'get_log error: filename =',filename,$
           ' should be a filename.'
     retall
  end

  TimeEvent    = ''
  NameLosTable = ''

  ObsXyz   = 0
  nvars    = 0
  data     = 0
  varnames = ''
  
  DoWl  = 0
  DoPb  = 0
  DoAIA = 0
  DoEUV = 0
  DoXRT = 0

  line  = ''
  nHeadline = 0
  IsHeader  = 1
  headlines = strarr(1)
  buf   = long(10000)
  dbuf  = long(10000)
  nt    = long(0)

  DoLoop = 1

  while not eof(unit) do begin
     if IsHeader then begin
        readf, unit, line
        IsHeader = 0

        ;; check if the line contains any character that is not a number 
        for i = 0, strlen(line)-1 do begin
           if strmatch(strmid(line,i,1), '[!    0123456789dDeENa \.+-]') $
           then begin
              IsHeader = 1
              break
           endif
        endfor

        ;; check if line contains a single number only
        if not IsHeader then begin
           n = 0
           string_to_array,line, numbers, n
           if n le 1 then IsHeader=1
        endif

        if IsHeader then begin
           if nHeadline eq 0 then begin
              headlines(0) = line 
           endif else begin
              headlines = [headlines, line]
           endelse
           nHeadline = nHeadline + 1
        endif else begin
           ;; Finished reading the header, now processing the header info
           ;; find variable names in the header lines
           for i = nHeadline - 1, 0, -1 do begin
              linetmp = headlines[i]

              ;; if the line contains 'VARIABLES'
              if (strpos(linetmp, 'VARIABLES') ge 0) then begin
                 ;; get the part after 'VARIABLES',
                 ;; which should be the var names
                 ii = strpos(linetmp, 'VARIABLES')
                 linetmp = strmid(linetmp, ii)
                 ii = strpos(linetmp, '=')
                 linetmp = strmid(linetmp,ii+1)

                 ;; get the string arrays, without " and ,
                 linetmp_I = strsplit(linetmp,'"',escape=',',/extract)

                 linetmp_I = strtrim(linetmp_I)

                 nnames   = 0
                 ;; maximum is 1000 var names.
                 varnames = strarr(1000)

                 for ivarname = 0,n_elements(linetmp_I)-1 do begin
                    if (strlen(linetmp_I(ivarname)) gt 0) then begin
                       varnames(nnames) = linetmp_I(ivarname)
                       nnames = nnames + 1
                    endif
                 endfor

                 varnames = varnames(0:nnames-1)
              endif

              if (strpos(linetmp, 'I=') ge 0) then begin
                 ;; get the part after 'I=',
                 ii = strpos(linetmp, 'I=')
                 linetmp = strmid(linetmp, ii)

                 ;; remove ','
                 linetmp = strjoin(strsplit(linetmp,',',/extract))

                 ii = strpos(linetmp, 'I=')
                 ij = strpos(linetmp, 'J=')
                 ik = strpos(linetmp, 'K=')
                 it = strpos(linetmp, 'F=')

                 if (ii lt 0 or ij lt 0 or ik le 0 or it lt 0) then begin
                    print, linetmp
                    print, ' The data file does not contain I=, J=, K=, F=?'
                    exit
                 endif

                 nx = fix(strmid(linetmp,ii+2,ij-1))
                 ny = fix(strmid(linetmp,ij+2,ik-1))
                 nz = fix(strmid(linetmp,ik+2,it-1))

                 if (nz ne 1) then begin
                    print, ' should be 2-D, why nz /= 1 ???'
                    print, linetmp
                    exit
                 endif
              endif

              if (strpos(linetmp, 'TIMEEVENT=') ge 0) then begin
                 ;; get the part after '='
                 ii = strpos(linetmp, '=')
                 linetmp = strmid(linetmp, ii+1)
                 linetmp = strjoin(strsplit(linetmp,'"',/extract))
                 TimeEvent = linetmp
              endif


              if (strpos(linetmp, 'TIMEEVENTSTART=') ge 0) then begin
                 ;; get the part after '='
                 ii = strpos(linetmp, '=')
                 linetmp = strmid(linetmp, ii+1)
                 linetmp = strjoin(strsplit(linetmp,'"',/extract))
                 
                 TimeEventStart = linetmp
              endif

              if (strpos(linetmp, 'NAMELOSTABLE=') ge 0) then begin
                 ;; get the part after '='
                 ii = strpos(linetmp, '=')
                 linetmp = strmid(linetmp, ii+1)
                 linetmp = strjoin(strsplit(linetmp,'"',/extract))
                 
                 ;; always in lower case
                 NameLosTable = strlowcase(linetmp)
              endif

              if (strpos(linetmp, 'HGIXYZ=') ge 0) then begin
                 ;; get the part after '='
                 ii = strpos(linetmp, '=')
                 linetmp = strmid(linetmp, ii+1)
                 linetmp = strjoin(strsplit(linetmp,'"',/extract))
                 
                 string_to_array, linetmp, ObsXyz

                 if (n_elements(ObsXyz) ne 3) then begin
                    print, linetmp
                    print, ' The observer position should contain only 3 vars'
                 endif
              endif
           endfor

           ;; The table name should be entered correctly!!!
           if (strpos(NameLosTable,'euvi') ge 0) then DoEUV = 1
           if (strpos(NameLosTable,'aia')  ge 0) then DoAIA = 1
           if (strpos(NameLosTable,'xrt')  ge 0) then DoXRT = 1
           if (strpos(NameLosTable,'eit')  ge 0) then DoEUV = 1

           ;; for pb/wl, no table names, but pb/wl appears in varnames
           tmp = where(strmatch(varnames,'*pb*',/fold_case), count)
           if (count ge 1) then DoPb  = 1
           tmp = where(strmatch(varnames,'*wl*',/fold_case), count)
           if (count ge 1) then DoWl  = 1


           ;; now read the first line of data
           ;; split line into numbers
           string_to_array,line, numbers, nvars

           if n_elements(varnames) ne nvars then begin
              print, ' varnames should have nVars string'
              print, ' varnames = ', varnames
              print, ' nvars    = ', nvars
              exit
           endif

           ;; create arrays to read data into
           data_ = dblarr(nvars)
           data  = dblarr(nx,ny,nvars)

           ;; read first line
           reads, line, data_
           if total(finite(data_)) eq nvars then begin
              data(0,0,*) = data_
              nt = 1L
           endif

        endelse
     endif else begin
        for ix = 0L,nx-1 do begin
           for iy = 0L,ny-1 do begin
              ;; already read the first one, start the next iteration
              if ((ix eq 0 and iy eq 0)) then continue

              readf, unit, data_

              if total(finite(data_)) ne nvars then begin
                 print, ' The number of data points /= nvars????'
                 exit
              endif

              data(ix,iy,*) = data_
              nt=nt+1
           endfor
        endfor
     endelse
  endwhile

  close,unit

  xx = data(*,*,0)
  yy = data(*,*,1)

  rr = sqrt(xx^2+yy^2)
  rMaxSim = max(rr)

  TimeSimulationSI = utc2tai(TimeEvent) - utc2tai(TimeEventStart)

  if (1L*nx*ny ne nt) then begin
     print, ' size does not match???'
     print, ' nx =', nx, ', ny =', ny, ', nt =', nt
     exit
  endif
end

;--------------------------------------------------------------------

pro read_swmf_sat, filename, time, ndens, ux, uy, uz, bx, by, bz, ti, te, $
                   ut, ur, bt, btotal, br, deltab, nvars, data, varnames, $
                   DoContainData=DoContainData, nData=nData,              $
                   TypeData=TypeData, TypePlot=TypePlot, start_time=start_time,   $
                   end_time=end_time, DoPlotDeltaB=DoPlotDeltaB

  DoContainData = 1
  TypeData = 'none'

  file_tmp = file_basename(filename)
  if (strpos(strlowcase(file_tmp), 'earth') ge 0) then begin
     TypeData      = 'OMNI'
     TypePlot  = '_omni'
  endif else if (strpos(strlowcase(file_tmp), 'sta')     ge 0 or $
                 strpos(strlowcase(file_tmp), 'stereoa') ge 0) then begin
     TypeData      = 'Stereo A'
     TypePlot  = '_sta'
  endif else if (strpos(strlowcase(file_tmp), 'stb')     ge 0 or $
                 strpos(strlowcase(file_tmp), 'stereob') ge 0) then begin
     TypeData      = 'Stereo B'
     TypePlot  = '_stb'
  endif else if (strpos(strlowcase(file_tmp), 'solo')    ge 0) then begin
     TypeData      = 'solo'
     TypePlot  = '_solo'
  endif else if (strpos(strlowcase(file_tmp), 'psp')    ge 0) then begin
     TypeData      = 'psp'
     TypePlot  = '_psp'
  endif
  
  itype = size(filename,/type)

  if itype eq 7 then begin
     unit  = 0
     found = 0
     while not found do begin
        unit = unit + 1
        stat = fstat(unit)
        if not stat.open then found = 1
     endwhile
     openr,unit,filename
  endif else begin
     print,'get_log error: filename =',filename,$
           ' should be a filename.'
     retall
  end

  line  = ''
  nHeadline = 0
  IsHeader  = 1
  headlines = strarr(1)
  buf   = long(10000)
  dbuf  = long(10000)
  nt    = long(0)

  while not eof(unit) do begin
     if IsHeader then begin
        readf, unit, line
        IsHeader = 0

        ;; check if the line contains any character that is not a number 
        for i = 0, strlen(line)-1 do begin
           if strmatch(strmid(line,i,1), '[!    0123456789dDeE \.+-]') $
           then begin
              IsHeader = 1
              break
           endif
        endfor

        ;; check if line contains a single number only
        if not IsHeader then begin
           n = 0
           string_to_array,line, numbers, n
           if n le 1 then IsHeader=1
        endif

        if IsHeader then begin
           if nHeadline eq 0 then begin
              headlines(0) = line 
           endif else begin
              headlines = [headlines, line]
           endelse
           nHeadline = nHeadline + 1
        endif else begin
           ;; if the line contines NaN, do not convert it into numbers.
           if strmatch(strmid(line,i,1), '[!    0123456789dDeE \.+-]') then begin
              print, " warning NaN found in the data"
              continue
           endif

           ;; split line into numbers
           string_to_array,line, numbers, nvars
           
           ;; create arrays to read data into
           data_ = dblarr(nvars)
           data  = dblarr(nvars,buf)

           ;; read first line
           reads, line, data_
           if total(finite(data_)) eq nvars then begin
              data(*,0) = data_
              nt = 1L
           endif

           ;; find variable names in the header lines
           for i = nHeadline - 1, 0, -1 do begin
              line = headlines[i]
              char = strlowcase(strmid(strtrim(line,1),0,1))
              if char ge 'a' and char le 'z' then begin
                 ;; Overwrite #START with spaces if present
                 j = strpos(line,'#START')
                 if j ge 0 then strput, line, '      ', j
                 
                 ;; split line into names
                 string_to_array, line, varnames, nnames

                 ;; if number of names agree we are done
                 if nnames eq nvars then BREAK
              endif
           endfor

           if n_elements(varnames) ne nvars then begin
              varnames = strarr(nvars)
              for i = 0, nvars - 1 do $
                 varnames[i] = 'var'+string(i, format='(i2.2)')
           endif

        endelse
     endif else begin
        readf, unit, data_
        if total(finite(data_)) eq nvars then begin
           data(*,nt) = data_
           nt=nt+1
        endif
        if nt ge buf then begin
           buf=buf+dbuf
           data=[[data],[dblarr(nvars,buf)]]
        endif
     endelse

  endwhile
  
  close,unit

  if nt lt 1 then begin
     DoContainData = 0
     return
  endif

  data = transpose(data(*,0:nt-1))

  DoEhot = 0
  DoI01 = 0
  DoI02 = 0
  
  for i = 0, nvars-1 do begin
     varname = strlowcase(varnames(i))
     case varname of
        'time'       : itime = i
        't'          : if wlognames(i) eq 't' then itime=i
        'step'       : istep = i
        'it'         : istep = i
        'year'       : iyear = i
        'yr'         : iyear = i
        'yy'         : iyear = i
        'month'      : imon  = i
        'mo'         : imon  = i
        'day'        : iday  = i
        'dy'         : iday  = i
        'dd'         : iday  = i
        'hour'       : ihour = i
        'hr'         : ihour = i
        'hh'         : ihour = i
        'minute'     : imin  = i
        'min'        : imin  = i
        'mn'         : imin  = i
        'mm'         : imin  = i
        'second'     : isec  = i
        'sec'        : isec  = i
        'sc'         : isec  = i
        'ss'         : isec  = i
        'millisecond': imsc  = i
        'millisec'   : imsc  = i
        'msecond'    : imsc  = i
        'msec'       : imsc  = i
        'msc'        : imsc  = i
        'x'          : ix    = i
        'y'          : iy    = i
        'z'          : iz    = i
        'rho'        : irho  = i
        'bx'         : ibx   = i
        'by'         : iby   = i
        'bz'         : ibz   = i
        'ux'         : iux   = i
        'uy'         : iuy   = i
        'uz'         : iuz   = i
        'p'          : ip    = i
        'pe'         : ipe   = i
        'ehot'       : begin
           iehot = i
           DoEhot = 1
        end
        'i01'        : begin
           ii01  = i
           DoI01 = 1
        end
        'i02'        : begin
           ii02  = i
           DoI02 = 1
        end
        else:
     endcase
  endfor

  year   =fix(data(*,iyear))
  month  =fix(data(*,imon))
  day    =fix(data(*,iday))
  hour   =fix(data(*,ihour))
  minute =fix(data(*,imin))
  x      =data(*,ix)
  y      =data(*,iy)
  z      =data(*,iz)
  rho    =data(*,irho)
  ux     =data(*,iux)
  uy     =data(*,iuy)
  uz     =data(*,iuz)
  bx     =data(*,ibx)
  by     =data(*,iby)
  bz     =data(*,ibz)
  p      =data(*,ip)            ;dyne/cm2
  pe     =data(*,ipe)
  if (DoEhot) then  ehot   =data(*,iehot)
  if (DoI01 && DoI02) then begin
     DoPlotDeltaB = 1
     i01    =data(*,ii01)
     i02    =data(*,ii02)       ; ergs/cc
  endif else DoPlotDeltaB = 0
  nData = n_elements(year)

  time=strarr(nData)

  for i=0,nData-1 do begin
     time[i]= strtrim(string(year[i],format='(i04)'),2)     +'-' $
              + strtrim(string(month[i],format='(i02)'),2)  +'-' $
              + strtrim(string(day[i],format='(i02)'),2)    +'T' $
              + strtrim(string(hour[i],format='(i02)'),2)   +':' $
              + strtrim(string(minute[i],format='(i02)'),2) +':00'
  endfor

  bt=sqrt(bx*bx+by*by+bz*bz)
  ut=sqrt(ux*ux+uy*uy+uz*uz)
  ur=(ux*x+uy*y+uz*z)/sqrt(x*x+y*y+z*z)
  br=(bx*x+by*y+bz*z)/sqrt(x*x+y*y+z*z)

  ProtonMass =1.67e-24          ;g

  ndens  = rho/ProtonMass

  k  = 1.3807e-23
  ti = p*ProtonMass/rho/k*1.e-7
  te = pe*ProtonMass/rho/k*1.e-7
  if (DoI01 && DoI02) then begin
     ew = i01 + i02             ;ergs/cc
     deltab = sqrt(4*!pi*ew)    ; Gauss
     deltau = sqrt(ew/rho)      ; cm/s
     btotal = sqrt(bt^2+deltab^2) ; Gauss
  endif
  start_time = time(0)
  end_time   = time(n_elements(time)-1)

end

;--------------------------------------------------------------------

function curve_int_distance,x1,y1,x2,y2
  
; Evaluates the distance between two curves (data & model results)
; independent of the coordinate system so that errors in x and y
; coordinates are treated equally.
; (x1,y1) & (x2,y2) represent the x,y coordinates curves 1 and 2
; respectively
; d1 is the average of the minimum distance between the two curves
; integrated along curve 1 and d2 is integrated along curve 2
; Function returns the error (distance) d, which is a symmetric
; function of the two curves.

  n1 = n_elements(x1)
  n2 = n_elements(x2)
  d1=0d0
  d2=0d0

  x1c= (x1(1:n1-1) + x1(0:n1-2))/2
  x2c= (x2(1:n2-1) + x2(0:n2-2))/2
  y1c= (y1(1:n1-1) + y1(0:n1-2))/2
  y2c= (y2(1:n2-1) + y2(0:n2-2))/2

  d1c = sqrt( (x1(1:n1-1) - x1(0:n1-2))^2 + (y1(1:n1-1) - y1(0:n1-2))^2 )
  d2c = sqrt( (x2(1:n2-1) - x2(0:n2-2))^2 + (y2(1:n2-1) - y2(0:n2-2))^2 )

  len1 = total(d1c)
  len2 = total(d2c)

  for i = 0, n1-2 do $
     d1 += d1c(i)*min( sqrt( (x1c(i) - x2c)^2 + (y1c(i) - y2c)^2 ) )
  for i = 0, n2-2 do $
     d2 += d2c(i)*min( sqrt( (x2c(i) - x1c)^2 + (y2c(i) - y1c)^2 ) )
  d = (d1/len1 + d2/len2)/2
  return, d
end

;------------------------------------------------------------------------------

function calc_dist_insitu,time_obs, u_obs, n_obs, T_obs,  B_obs,    $
                          time_simu,u_simu,n_simu,ti_simu,b_simu,   $
                          dist_int_u, dist_int_t, dist_int_n, dist_int_b, $
                          EventTimeDist, TimeWindowDist

  ;; Converting swmf time (YYYY-MO-DDTHH:MM:SS) to epoch time in milliseconds
  TIMESTAMPTOVALUES,time_simu+'Z',year=yy,month=mo,day=dy,hour=hh,min=mm,sec=ss
  cdf_epoch,time_simu_local,yy,mo,dy,hh,mm,ss,/compute_epoch ; in milliseconds

  u_obs_local = u_obs
  n_obs_local = n_obs
  T_obs_local = T_obs
  B_obs_local = B_obs
  time_obs_local = time_obs

  u_simu_local  = u_simu
  n_simu_local  = n_simu
  ti_simu_local = ti_simu
  b_simu_local  = b_simu

  ; Normalization time for 10 days by default
  t_norm=10

  ;; set the time range for calculating the distance if needed...
  if EventTimeDist ne 'none' then begin
     ;; set new normalization time
     t_norm = floor(abs(TimeWindowDist)/2)

     TIMESTAMPTOVALUES,EventTimeDist+'Z',year=yy_d,month=mo_d,day=dy_d, $
                       hour=hh_d,min=mm_d,sec=ss_d
     cdf_epoch,EventTimeDist_epo,yy_d,mo_d,dy_d,hh_d,mm_d,ss_d,/compute_epoch

     ;; TimeWindowDist in days, but epoch time is in milliseconds
     TimeDistStart_epo = min([EventTimeDist_epo, $
                              EventTimeDist_epo+TimeWindowDist*24.*60.*60.*1e3])
     TimeDistEnd_epo   = max([EventTimeDist_epo, $
                              EventTimeDist_epo+TimeWindowDist*24.*60.*60.*1e3])

     ;; extract observation during the time range
     index_tmp = where(time_obs_local ge TimeDistStart_epo and time_obs_local le TimeDistEnd_epo)
     u_obs_local = u_obs(index_tmp)
     n_obs_local = n_obs(index_tmp)
     T_obs_local = T_obs(index_tmp)
     B_obs_local = B_obs(index_tmp)
     time_obs_local = time_obs_local(index_tmp)

     ; cdf_epoch,time_obs_local(0),y1,m1,d1,h1,min1,s1,ss1,/break
     ; print,y1,m1,d1,h1,min1,s1,ss1
     ; cdf_epoch,time_obs_local(-1),y1,m1,d1,h1,min1,s1,ss1,/break
     ; print,y1,m1,d1,h1,min1,s1,ss1

     ;; extract simulation during the time range
     index_tmp = where(time_simu_local ge TimeDistStart_epo and time_simu_local le TimeDistEnd_epo)
     u_simu_local  = u_simu(index_tmp)
     n_simu_local  = n_simu(index_tmp)
     ti_simu_local = ti_simu(index_tmp)
     b_simu_local  = b_simu(index_tmp)
     time_simu_local = time_simu_local(index_tmp)

     ; cdf_epoch,time_simu_local(0),y1,m1,d1,h1,min1,s1,ss1,/break
     ; print,y1,m1,d1,h1,min1,s1,ss1
     ; cdf_epoch,time_simu_local(-1),y1,m1,d1,h1,min1,s1,ss1,/break
     ; print,y1,m1,d1,h1,min1,s1,ss1
  endif

  index_u = where(u_obs_local gt 0)
  index_n = where(n_obs_local gt 0)
  index_T = where(T_obs_local gt 0)
  index_B = where(B_obs_local gt 0)

  ;;Normalization for OMNI data
  u_norm   = max(u_obs_local) - min(u_obs_local(index_u))
  n_norm   = max(n_obs_local) - min(n_obs_local(index_n))
  tem_norm = max(T_obs_local) - min(T_obs_local(index_T))
  mag_norm = max(B_obs_local) - min(B_obs_local(index_B))

  ;;time in units of t_norm days, initially in milliseconds
  time_simu_local = time_simu_local/(24.*60.*60.*1e3)/t_norm
  time_obs_local  = time_obs_local /(24.*60.*60.*1e3)/t_norm

  dist_int_u=curve_int_distance(time_obs_local(index_u), u_obs_local(index_u)/u_norm,   $
                                time_simu_local, u_simu_local/u_norm)
  dist_int_t=curve_int_distance(time_obs_local(index_T), T_obs_local(index_T)/tem_norm, $
                                time_simu_local, ti_simu_local/tem_norm)
  dist_int_n=curve_int_distance(time_obs_local(index_n), n_obs_local(index_n)/n_norm,   $
                                time_simu_local, n_simu_local/n_norm)
  dist_int_b=curve_int_distance(time_obs_local(index_B), B_obs_local(index_B)/mag_norm, $
                                time_simu_local, b_simu_local*1.e5/mag_norm)

  dist_int=['Dist_U ='+STRING(trim(dist_int_u),format='(f6.3)'),$
            'Dist_N ='+STRING(trim(dist_int_n),format='(f6.3)'),$
            'Dist_T ='+STRING(trim(dist_int_t),format='(f6.3)'),$
            'Dist_B ='+STRING(trim(dist_int_b),format='(f6.3)')]

  ;; print, dist_int
  return,dist_int
end

;------------------------------------------------------------------------------

pro plot_insitu, time_obs,  u_obs,  n_obs,  T_obs,   B_obs,                   $
                 time_simu, u_simu, n_simu, ti_simu, te_simu, b_simu,         $
                 btotal_simu, deltab_simu, start_time, end_time,              $
                 fileplot=fileplot,                                           $
                 typeData=typeData, charsize=charsize, colorLocal=colorLocal, $
                 DoPlotTe=DoPlotTe, legendNames = legendNames,                $
                 legendPosL=legendPosL, legendPosR=legendPosR,                $
                 DoShowDist=DoShowDist, DoLogRho=DoLogRho, DoLogT=DoLogT,     $
                 IsOverPlot=IsOverPlot, DoLegend=DoLegend,                    $
                 ymin_I=ymin_I, ymax_I=ymax_I, linethick=linethick,           $
                 nLegendPlot=nLegendPlot,file_dist=file_dist,                 $
                 EventTimeDist=EventTimeDist, TimeWindowDist=TimeWindowDist,  $
                 DoPlotDeltaB=DoPlotDeltaB, start_time_CME_I=start_time_CME_I,$
                 end_time_CME_I  =end_time_CME_I

  if (not isa(DoPlotTe)) then DoPlotTe = 1
  
  if (not isa(legendNames)) then begin
     if DoPlotTe then begin
        legendNamesLocal = ['AWSoM', 'AWSoM Te']
     endif else begin
        legendNamesLocal = 'AWSoM'
     endelse
  endif else begin
     if DoPlotTe then begin
        legendNamesLocal = [legendNames, legendNames+' Te']
     endif else begin
        legendNamesLocal = [legendNames]
     endelse
  endelse

  if (not isa(IsOverPlot)) then IsOverPlot = 0
  if (not isa(DoShowDist)) then DoShowDist = 1
  if (not isa(legendPosL)) then legendPosL = [0.15,0.95]
  if (not isa(legendPosR)) then legendPosR = 0.94
  if (not isa(DoLogRho))   then DoLogRho   = 0
  if (not isa(DoLogT))     then DoLogT     = 0
  if (not isa(colorLocal)) then colorLocal = 5
  if (not isa(DoLegend))   then DoLegend   = 1
  if (not isa(linethick))  then linethick  = 9

  if (not isa(start_time_CME_I)) then start_time_CME_I = ''
  if (not isa(end_time_CME_I))   then end_time_CME_I = ''

  if (not keyword_set(ymin_I)) then ymin_I = [200,0,0,0]

  if DoLogT then ymin_I[2]=1e3

  if IsOverPlot then begin
     if (not isa(DoShowDist)) then DoShowDist = 0
  endif else begin
     if (not isa(DoShowDist)) then DoShowDist = 1
  endelse

  if IsOverPlot ne 1 then legendNamesLocal = [typeData, legendNamesLocal]

  if DoLegend then begin
     nLegendPlot = n_elements(legendNamesLocal)
  endif else begin
     nLegendPlot = 0
  endelse

  index_u = where(u_obs gt 0)
  index_n = where(n_obs gt 0)
  index_T = where(T_obs gt 0)
  index_B = where(B_obs gt 0)

  if (not keyword_set(ymax_I)) then $
     ymax_I = [max([max(u_obs(index_u)), max(u_simu)])*1.3,      $
               max([max(n_obs[index_n]), max(n_simu)])*1.3,      $
               max([max(T_obs[index_T]), max(ti_simu)])*1.3,     $
               max([max(B_obs[index_B]), max(b_simu)*1.e5])*1.3]
  
  utc_obs = anytim2utc(cdf2utc(time_obs),/external)

  ;; utc_obs is in epoch time already, convet the simulation time
  TIMESTAMPTOVALUES,time_simu+'Z',year=yy,month=mo,day=dy,hour=hh,min=mm,sec=ss
  cdf_epoch,time_simu_epoch,yy,mo,dy,hh,mm,ss,/compute_epoch

  if (start_time_CME_I(0)) then begin
     TIMESTAMPTOVALUES,start_time_CME_I+'Z',year=yy,month=mo,day=dy,$
                       hour=hh,min=mm,sec=ss
     cdf_epoch,start_time_CME_epoch_I,yy,mo,dy,hh,mm,ss,/compute_epoch

     TIMESTAMPTOVALUES,end_time_CME_I+'Z',year=yy,month=mo,day=dy,$
                       hour=hh,min=mm,sec=ss
     cdf_epoch,end_time_CME_epoch_I,yy,mo,dy,hh,mm,ss,/compute_epoch
  endif

  index_cme = -1

  ;; the start/end time of the plot is obtaind from the simulation, as the data is
  ;; downloaded based on the start/end time of the simulation.
  start_time_epoch = time_simu_epoch(0)
  end_time_epoch   = time_simu_epoch(-1)
  
  for i=0,n_elements(start_time_CME_epoch_I)-1 do begin
     start_time_cme_local = start_time_CME_epoch_I(i)
     end_time_cme_local   = end_time_CME_epoch_I(i)
     ;; 1. CME is fully within the plot
     ;; 2. the beginning is within the plot
     ;; 3. the end is within the plot
     if ((start_time_cme_local ge start_time_epoch and $
          start_time_cme_local le end_time_epoch) or $
         (end_time_cme_local ge start_time_epoch and $
          end_time_cme_local le end_time_epoch)) then begin
        ;; add to the array
        index_cme = [index_cme,i]
     endif
  endfor

  index_obs = -1
  index_sim = -1
  
  if n_elements(index_cme) gt 1 then begin
     index_cme = index_cme[1:-1]
     ;; extract CME indices for both observations & sims
     index_obs = -1
     index_sim = -1
     ;; print the CME info
     for i=0,n_elements(index_cme)-1 do begin
        print, 'CME is between ', start_time_CME_I(index_cme(i)),$
               ' and ', end_time_CME_I(index_cme(i))
        index_obs = [index_obs,$
                     where(time_obs ge start_time_CME_epoch_I(index_cme(i)) $
                           and time_obs le end_time_CME_epoch_I(index_cme(i)))]
        index_sim = [index_sim, where(time_simu_epoch ge $
                                      start_time_CME_epoch_I(index_cme(i)) $
                                      and time_simu_epoch le $
                                      end_time_CME_epoch_I(index_cme(i)))]
     endfor
  endif
;; the first element is -1, need to be removed
  if n_elements(index_obs) gt 1 then begin
     index_obs = index_obs[1:-1]
     index_sim = index_sim[1:-1]
  endif

  ; extracting non-cme indices from obs and sims
  time_obs_local = time_obs
  time_obs_local(index_obs) = -1
  index_nocme = where(time_obs_local gt -1) 
  time_obs_local = time_obs[index_nocme]
  u_obs_local = u_obs[index_nocme]
  n_obs_local = n_obs[index_nocme]
  T_obs_local = T_obs[index_nocme]
  B_obs_local = B_obs[index_nocme]
  
  time_simu_local = time_simu
  time_simu_local(index_sim) = -1
  index_nocme= where(time_simu_local gt -1) 
;  print,'Final Indices for SIM =',index_nocme
  time_simu_local = time_simu[index_nocme]
  u_simu_local = u_simu[index_nocme]
  n_simu_local = n_simu[index_nocme]
  ti_simu_local = ti_simu[index_nocme]
  btotal_simu_local = btotal_simu[index_nocme]
  b_simu_local = b_simu[index_nocme]

  if DoShowDist then begin
     if (DoPlotDeltaB) then begin
        print,'Dist is calculated for regions outside the CME'
        dist_int = calc_dist_insitu(time_obs_local, u_obs_local, n_obs_local,$
                                    T_obs_local, B_obs_local,     $
                                    time_simu_local,u_simu_local,n_simu_local,$
                                    ti_simu_local,          $
                                    btotal_simu_local,                     $
                                    dist_int_u, dist_int_t,                   $
                                    dist_int_n, dist_int_b,                   $
                                    EventTimeDist, TimeWindowDist)
     endif else begin
        dist_int = calc_dist_insitu(time_obs_local, u_obs_local, n_obs_local,$
                                    T_obs_local, B_obs_local,     $
                                    time_simu_local,u_simu_local,n_simu_local,$
                                    ti_simu_local,b_simu_local,   $
                                    dist_int_u, dist_int_t,                   $
                                    dist_int_n, dist_int_b,                   $
                                    EventTimeDist, TimeWindowDist)
     endelse
     openw, lun_dist, file_dist, /get_lun
     printf,lun_dist, 'Dist_U ='+STRING(trim(dist_int_u),format='(f7.4)')
     printf,lun_dist, 'Dist_N ='+STRING(trim(dist_int_n),format='(f7.4)')
     printf,lun_dist, 'Dist_T ='+STRING(trim(dist_int_t),format='(f7.4)')
     printf,lun_dist, 'Dist_B ='+STRING(trim(dist_int_b),format='(f7.4)')
     close, lun_dist
     free_lun,lun_dist
  endif

  loadcolors 

  nx=1 & ny=4
  if IsOverPlot ne 1 then !p.multi=[0,1,4]
  cal_pos,nx,ny,y_x_ratio=y_x,x1=x1,y1=y1,x2=x2,y2=y2,$
          dx=0.4,dy=0.2,bm=0.9,tm=0.35,lm=0.4,rm=0.2

  ;;----------------------------------------------------------------------
  ;; plot velocity

  ymin = ymin_I[0]
  ymax = ymax_I[0]

  pos=[x1[0],y1[3],x2[0],y2[3]]

  if IsOverPlot ne 1 then begin
     if max(index_cme) ge 0 then begin
        ;; complicated to plot the CME interval due to the transparency...
        ;; create a plot without any actual line first
        utplot,utc_obs(index_u),u_obs(index_u),background=7,    $
               timerange=[start_time,end_time],                 $
               xstyle=5,yrange=[ymin,ymax],ystyle=5,                         $
               position=pos
        ;; plot the CME interval
        plot_CME_interval, start_time_CME_epoch_I, end_time_CME_epoch_I, index_cme, $
                           start_time_epoch, end_time_epoch, ymin, ymax
        ;; plot the actual data
        utplot,utc_obs(index_u),u_obs(index_u),background=7,color=0,         $
               ytitle='U [km/s]',thick=9, timerange=[start_time,end_time],  $
               xstyle=1,yrange=[ymin,ymax],ystyle=1,                         $
               charsize=charsize,charthick=5,xthick=5,ythick=5,position=pos, $
               xtickname=REPLICATE(' ', 7),xtitle=' ', /noerase
     endif else begin
        utplot,utc_obs(index_u),u_obs(index_u),background=7,color=0,         $
               ytitle='U [km/s]',thick=9, timerange=[start_time,end_time],  $
               xstyle=1,yrange=[ymin,ymax],ystyle=1,                         $
               charsize=charsize,charthick=5,xthick=5,ythick=5,position=pos, $
               xtickname=REPLICATE(' ', 7),xtitle=' '
     endelse
  endif

  utplot,time_simu, u_simu, background=7, color=colorLocal,               $
         thick=linethick, timerange=[start_time,end_time],                $
         yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase
  
  if DoLegend then begin
     if (DoPlotTe) then begin
        if IsOverPlot ne 1 then begin
           legend,legendNamesLocal, colors=[0,colorLocal,colorLocal],       $
                  psym=0,textcolor=0,thick=6,                               $
                  linestyle=[0,0,2],                                        $
                  charsize=1,pspacing=1.8,charthick=5,bthick=5,             $
                  position=legendPosL,/norm,box=0
        endif else begin
           legend,legendNamesLocal, colors=[colorLocal,colorLocal],        $
                  psym=0,textcolor=0,thick=6,                              $
                  linestyle=[0,2],                                         $
                  charsize=1,pspacing=1.8,charthick=5,bthick=5,            $
                  position=legendPosL,/norm,box=0
        endelse        
     endif else begin
        if IsOverPlot ne 1 then begin
           legend,legendNamesLocal, colors=[0, colorLocal],                 $
                  psym=0,textcolor=0,thick=6,linestyle=0,                   $
                  charsize=1,pspacing=1.8,charthick=5,bthick=5,             $
                  position=legendPosL, /norm,box=0
        endif else begin
           legend,legendNamesLocal, colors=colorLocal,                      $
                  psym=0,textcolor=0,thick=6,linestyle=0,                   $
                  charsize=1,pspacing=1.8,charthick=5,bthick=5,             $
                  position=legendPosL,/norm,box=0
        endelse        
     endelse
  endif else if IsOverPlot ne 1 then begin
     ;; print typeData as legend even if DoLegend = 0 but the plot is
     ;; not over-plot
     legend,typeData, colors=0,                                       $
            psym=0,textcolor=0,thick=6,linestyle=0,                   $
            charsize=1,pspacing=1.8,charthick=5,bthick=5,             $
            position=legendPosL, /norm,box=0
     nLegendPlot = 1
  endif

  if DoShowDist ne 0 then legend,dist_int(0),thick=6,charsize=1,charthick=5,  $
                                 position=[0.75,legendPosR], /norm, box=0
  
  ;;----------------------------------------------------------------------
  ;; plot density

  ymin = ymin_I[1]
  ymax = ymax_I[1]

  pos=[x1[0],y1[2],x2[0],y2[2]]

  if IsOverPlot ne 1 then begin
     if max(index_cme) ge 0 then begin
        utplot,utc_obs[index_n],n_obs[index_n],background=7,                 $
               timerange=[start_time,end_time],xstyle=5,                     $
               position=pos, yrange=[ymin,ymax], ystyle=5,/nodata
        plot_CME_interval, start_time_CME_epoch_I, end_time_CME_epoch_I, index_cme, $
                           start_time_epoch, end_time_epoch, ymin, ymax
        utplot,utc_obs[index_n],n_obs[index_n],background=7,color=0,         $
               ytitle='Np [cm!E-3!N]',thick=9,                               $
               timerange=[start_time,end_time],xstyle=1,charsize=charsize,   $
               charthick=5,xthick=5,                                         $
               ythick=5,position=pos,xtickname=REPLICATE(' ', 7),xtitle=' ', $
               yrange=[ymin,ymax], ystyle=1,/noerase
     endif else begin
        utplot,utc_obs[index_n],n_obs[index_n],background=7,color=0,         $
               ytitle='Np [cm!E-3!N]',thick=9,                               $
               timerange=[start_time,end_time],xstyle=1,charsize=charsize,   $
               charthick=5,xthick=5,                                         $
               ythick=5,position=pos,xtickname=REPLICATE(' ', 7),xtitle=' ', $
               yrange=[ymin,ymax], ystyle=1
     endelse
  endif
     
  utplot,time_simu, n_simu, background=7, color=colorLocal,               $
         thick=linethick, timerange=[start_time,end_time],                $
         yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase

  if DoShowDist ne 0 then legend,dist_int(1),thick=5,charsize=1,charthick=5, $
                                 position=[0.75,legendPosR-0.22],/norm,box=0

  ;;----------------------------------------------------------------------
  ;; plot temperature
  ymin = ymin_I[2]
  ymax = ymax_I[2]

  pos=[x1[0],y1[1],x2[0],y2[1]]

  if IsOverPlot ne 1 then begin
     if max(index_cme) ge 0 then begin
        utplot,utc_obs[index_T],T_obs[index_T],background=7,                      $
               timerange=[start_time,end_time], xstyle=5,                         $
               position=pos, yrange=[ymin,ymax],ystyle=5, ytype=DoLogT,/nodata
        plot_CME_interval, start_time_CME_epoch_I, end_time_CME_epoch_I, index_cme, $
                           start_time_epoch, end_time_epoch, ymin, ymax
        utplot,utc_obs[index_T],T_obs[index_T],background=7,color=0,              $
               ytitle='Temperature [K]',thick=9,timerange=[start_time,end_time],  $
               xstyle=1,                                                          $
               charsize=charsize,charthick=5,xthick=5,ythick=5,position=pos,      $
               xtickname=REPLICATE(' ', 7),xtitle=' ',yrange=[ymin,ymax],ystyle=1,$
               ytype=DoLogT, /noerase
     endif else begin
        utplot,utc_obs[index_T],T_obs[index_T],background=7,color=0,              $
               ytitle='Temperature [K]',thick=9,timerange=[start_time,end_time],  $
               xstyle=1,                                                          $
               charsize=charsize,charthick=5,xthick=5,ythick=5,position=pos,      $
               xtickname=REPLICATE(' ', 7),xtitle=' ',yrange=[ymin,ymax],ystyle=1,$
               ytype=DoLogT
     endelse
  endif
  utplot,time_simu, ti_simu, background=7, color=colorLocal,              $
         thick=linethick, timerange=[start_time,end_time],                $
         yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase,    $
         ytype=DoLogT

  if (DoPlotTe) then begin
     utplot,time_simu, te_simu, background=7, color=colorLocal,              $
            thick=linethick, timerange=[start_time,end_time],                $
            yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase,    $
            ytype=DoLogT, linestyle=2
  endif
  if DoShowDist ne 0 then legend,dist_int(2),thick=5,charsize=1,charthick=5, $
                                 position=[0.75,legendPosR-0.45],/norm,box=0

  ;;----------------------------------------------------------------------
  ;; plot magnetic field

  ymin = ymin_I[3]
  ymax = ymax_I[3]

  pos=[x1[0],y1[0],x2[0],y2[0]]
  if (DoPlotDeltaB) then begin
     ytitle_b = 'B+deltaB [nT]'
  endif else ytitle_b = 'B [nT]'
  if IsOverPlot ne 1 then begin
     if max(index_cme) ge 0 then begin
        utplot,utc_obs(index_B),B_obs(index_B),background=7,                    $
               timerange=[start_time,end_time],xstyle=5,yrange=[ymin,ymax],     $
               ystyle=5, position=pos, /nodata
        plot_CME_interval, start_time_CME_epoch_I, end_time_CME_epoch_I, index_cme, $
                           start_time_epoch, end_time_epoch, ymin, ymax
        utplot,utc_obs(index_B),B_obs(index_B),background=7,color=0,            $
               ytitle=ytitle_b,thick=9,                                         $
               timerange=[start_time,end_time],xstyle=1,yrange=[ymin,ymax],     $
               ysytle=1, charsize=charsize,                                     $
               charthick=5,xthick=5,ythick=5,position=pos,/noerase
     endif else begin
        utplot,utc_obs(index_B),B_obs(index_B),background=7,color=0,            $
               ytitle=ytitle_b,thick=9,                                         $
               timerange=[start_time,end_time],xstyle=1,yrange=[ymin,ymax],     $
               ystyle=1, charsize=charsize,                                     $
               charthick=5,xthick=5,ythick=5,position=pos
     endelse
  endif
  if (DoPlotDeltaB) then begin
     utplot,time_simu, btotal_simu*1.e5, background=7, color=colorLocal,$
            thick=linethick, timerange=[start_time,end_time],                $
            yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase,    $
            linestyle=0
     utplot,time_simu, deltab_simu*1.e5, background=7, color=colorLocal,     $
            thick=linethick, timerange=[start_time,end_time],linestyle=2,    $
            yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase     
  endif else begin
     utplot,time_simu, b_simu*1.e5, background=7, color=colorLocal,             $
            thick=linethick, timerange=[start_time,end_time],                   $
            yrange=[ymin,ymax],xstyle=5,ystyle=5, position=pos, /noerase
  endelse
  if DoShowDist ne 0 then legend,dist_int(3),thick=5,charsize=1,charthick=5,  $
                                 position=[0.75,legendPosR-0.67],/norm,box=0
end

;--------------------------------------------------------------------

pro download_images,  TimeEvent = TimeEvent, CaseInst = CaseInst,    $
                      SourceName = SourceName, InstName = InstName,  $
                      DetName = DetName, InstPlt = InstPlt,          $
                      filename_I = filename_I, unitlog = unitlog,    $
                      dir_obs = dir_obs, TimeStrFile = TimeStrFile

  NameSub = 'download_images'

  case CaseInst of
     'aia': begin
        TimeWin    = 0.1*3600
        nBand      = 7
        Band_I     = ['94','131','171','193','211','304','335']
        InstPlt    = 'AIA'
     end
     'euv': begin
        TimeWin    = 8.0*3600
        nBand      = 3
        Band_I     = ['171','195','284']
     end
  endcase

  index_I    = intarr(nBand)

  filenameOrig_I = strarr(nBand)
  filename_I     = strarr(nBand)

  ;; default is to download all the bands
  DoDownload_I = intarr(nBand) + 1

  ;; default is the file exists on the server
  DoFileExistServer_I = intarr(nBand) + 1

  TimeEventT = utc2tai(anytim2utc(TimeEvent))
  TimeStart  = strjoin(strsplit( utc2str( tai2utc(TimeEventT-TimeWin) ), '-', $
                                 /extract) , '/')
  TimeEnd    = strjoin(strsplit( utc2str( tai2utc(TimeEventT+TimeWin) ), '-', $
                                 /extract) , '/')

  TimeRange = TimeStart + '-' + TimeEnd
  print,'TimeRange =',TimeRange
  case CaseInst of
     'aia': $
        ListLocal_I = vso_search(date=TimeRange, source='SDO', det='aia', $
                                 wave='94-335 Angstrom', COUNT=nFiles)
     'euv': $
        ListLocal_I = vso_search(date=TimeRange, source=SourceName,      $
                                 inst=InstName,  det=DetName, COUNT=nFiles)
  endcase

  if (nFiles eq 0) then begin
     printf, unitlog, '*******************************************************'
     printf, unitlog, ' cannot find any data in the server !!!!!!'
     printf, unitlog, '*******************************************************'
     return
  endif

  ListTimeLocal_I = utc2tai(ListLocal_I.Time.Start)

  for iBand=0, nBand-1 do begin
     indexTmp_I = where(ListLocal_I.Wave.Min eq Band_I[iBand] and  $
                        ListLocal_I.Size gt 600., Count)

     if Count lt 1 then indexTmp_I = where(ListLocal_I.Wave.Min eq $
                                           Band_I[iBand], Count)

     if Count lt 1 then begin
        printf, unitlog, NameSub, ': ERROR!!!!!'
        printf, unitlog, NameSub, ': could not find an image for band: ', $
                Band_I[iBand]
        printf, unitlog, '           for time range: ', TimeRange

        ;; the file does not exist on the server
        DoFileExistServer_I(iBand)    = 0
        index_I(iBand)          = -1
        filenameOrig_I(iBand)   = ''
        continue
     endif

     ObsTimeBand_I  = ListTimeLocal_I[indexTmp_I]
     indexTempLocal = where(abs(ListTimeLocal_I   - TimeEventT)         $
                            eq min(abs(ObsTimeBand_I - TimeEventT))     $
                            and ListLocal_I.Wave.Min eq Band_I[iBand])


     index_I[iBand] = indexTempLocal[0]

     StringLine = strsplit(ListLocal_I[index_I[iBand]].fileid,'/', /extract)
     nStrings   = n_elements(StringLine)

     ;; get the original file name, which may change after calling vso_get
     filenameOrig_I(iBand) = StringLine[nStrings-1]
  endfor

  GoodList_I = ListLocal_I(index_I)

  files_filename_save = dir_obs + '/filenames_' + TimeStrFile + $
                        '_' + InstPlt +'.sav'

  if (file_test(files_filename_save)) then begin
     restore, files_filename_save

     for iBand = 0, nBand - 1 do begin

        if (filenameSave_I(iBand) eq '') then begin
           ;; try to download if filenameSave_I(iBand) is empty, which
           ;; indicates something goes wrong last time
           DoDownload_I(iBand) = 1
        endif else begin
           ;; set the filename_I first
           filename_I(iBand) = filenameSave_I(iBand)

           ;; try to download if the filenameOrig_I changes (upddated file?) or
           ;; the downloaded filename_I is deleted
           DoDownload_I(iBand) =                                    $
              filenameOrigSave_I(iBand) ne filenameOrig_I(iBand) or $
              (not file_test(filename_I(iBand)))
        endelse
     endfor
  endif

  for iBand = 0, nBand - 1 do begin
     if (DoFileExistServer_I(iBand)) then begin
        if (DoDownload_I(iBand)) then begin
           nTry = 1
           Status = vso_get(GoodList_I(iBand),out_dir=dir_obs, $
                            FILENAMES=filenametmp)

           while (filenametmp eq '' and nTry le 5) do begin
              printf, unitlog, NameSub + ': download failed '+InstPlt + ' ' + $
                      Band_I(iBand) + ' for nTry = '+string(nTry,format='(i2)')
              printf, unitlog, NameSub + ': wait one minute................'
              wait, 60

              Status = vso_get(GoodList_I(iBand), out_dir=dir_obs, $
                               FILENAMES=filenametmp)
              nTry += 1
           endwhile

           if (filenametmp ne '') then begin
              printf, unitlog, NameSub + ': downloaded ' +  InstPlt + ' '   + $
                      Band_I(iBand) + ': ' + filenametmp
           endif else begin
              printf, unitlog, NameSub + ': download failed '+InstPlt + ' ' + $
                      Band_I(iBand) + ' for five times, try again later.'
           endelse

           filename_I(iBand) = filenametmp
        endif else begin
           printf, unitlog, NameSub + ': ' + InstPlt + ' ' + Band_I(iBand) + $
                   ' existed at ' + filename_I(iBand)
        endelse
     endif else begin
        printf, unitlog, NameSub+': ' + InstPlt + ' ' + Band_I(iBand)      + $
                ' does not exist on the server.'
        filename_I(iBand) = ''
     endelse
  endfor

  printf, unitlog, ''

  filenameSave_I     = filename_I
  filenameOrigSave_I = filenameOrig_I

  save, filenameSave_I, filenameOrigSave_I, filename=files_filename_save
end

;--------------------------------------------------------------------

PRO loadcolors, bottom=bottom, names=names

;copied from "Practical IDL Programing', by Liam Gumley
;
;example:
;   IDL> loadcolors
;   IDL> x=findgen(200)*0.1
;   IDL> plot,x,sin(x),color=4

;check arguments
IF (n_elements(bottom) EQ 0) THEN bottom=0

;load graphics colors
red=[0,255,0,255,0,255,0,255,0,255,255,112,219,127,0,255]
grn=[0,0,255,255,255,0,0,255,0,187,127,219,112,127,163,171]
blu=[0,255,255,0,0,0,255,255,115,0,127,147,219,127,255,127]
tvlct,red,grn,blu

;set color names
names=['black','magenta','cyan','yellow','green','red','blue','white', $
       'navy','gold','pink','aquamarine','orchid','gray','sky','beige']

end

;--------------------------------------------------------------------

function rms_error, obs_data, simu_data

; for a 1 D array
diff_data = 0.
obs_sum = 0.
rmse= 0.
count = n_elements(obs_data)
for j=0, count-1 do diff_data += ((simu_data(j) - obs_data(j))/obs_data(j))
rmse = sqrt(((diff_data)^2)/count)

return, rmse

end

;--------------------------------------------------------------------

pro quantify_los, obs_data, sim_data, $
               DoDiffMap=DoDiffMap, DoRatioMap=DoRatioMap, DoRmse=DoRmse,$
               unitlog=unitlog

index = where(obs_data.data lt 0)
;print,obs_data.data[index],index
obs_data.data[index] = 0.
;print,max(obs_data.data),min(obs_data.data)
;print,obs_data.data[200,200]

; Calculate the no. of elements
obs_size = size(obs_data.data);,/n_elements)
sim_size = size(sim_data.data);,/n_elements)

;print,'Size of OBS  is = ', obs_size[4]
;print,'Size of SWMF is = ', sim_size[4]

if (obs_size[4] ne sim_size[4]) then begin
   print, 'Data and Model output are of different sizes'
   exit
endif

;print,'total',total(obs_data.data),total(sim_data.data)
mean_obs = total(obs_data.data)/obs_size[4]
mean_sim = total(sim_data.data)/sim_size[4]

ratio = mean_obs/mean_sim
;print,'Mean value of Observations is = ',trim(mean_obs),$
     ; ' and Simulation is = ',trim(mean_sim)
;print,'Ratio: Mean_obs/Mean_sim = ',ratio

printf,unitlog,'Mean_obs = ',trim(mean_obs)
printf,unitlog,'Mean_sim = ',trim(mean_sim)
printf,unitlog,'Ratio (Mean_obs/Mean_sim) = ',trim(ratio)

; SAVE PLOTS HERE ?
if keyword_set(DoDiffMap) then begin
   d_map = diff_map(obs_data,sim_data)
   plot_map,d_map
endif
if keyword_set(DoRatioMap) then begin
   d_map_ratio = diff_map(obs_data,sim_data,ratio=ratio)
   plot_map,d_map_ratio
endif

sq_error = 0.
rmse = 0.
nrmse = 0. ; normalised rmse
scatter = 0.
i=0.
j=0.

if keyword_set(DoRmse) then begin
   for i=0,obs_size[1]-1 do begin
      for j=0,obs_size[1]-1 do begin
         sq_error = sq_error + (sim_data.data[i,j] - obs_data.data[i,j])^2
      endfor
   endfor
   rmse = sqrt(sq_error/obs_size[4])
   scatter = rmse/mean_obs
   nrmse = rmse/(max(obs_data.data) - min(obs_data.data))

   printf,unitlog,'Root mean square error (RMSE) = ',trim(rmse)
   printf,unitlog,'Scatter = RMSE/(mean_obs)= ',trim(scatter)
   printf,unitlog,'Normalized RMSE (norm to max(obs) - min(obs)) = ',$
          trim(nrmse)
endif
end

;--------------------------------------------------------------------

pro compare_IDL_plot,file_obs,file_sim,file_plot,unitlog=unitlog
  common getpict_param
  common file_head
  common plotfunc_param
  common plot_param
  common plot_data
  common animate_param
  print,'Plotting IDL comparisons'
  filename+=' '+file_obs
  read_data
  !x.range=[-1.2,1.2]
  !y.range=[-1.2,1.2]
  multiplot=[3,4,0]
  func='AIA:94 AIA:131 AIA:171 AIA:193 AIA:211 AIA:335'
  autorange='n'
  fmin=fltarr(nw-1)+0.01
  fmax=dblarr(nw-1)
  for i=0,5 do begin
     if i eq 5 then begin
;; not plotting 304 just to fit 6 plots. Can be changed !
        fmax(i)=max([max(w0(*,*,i+1)),max(w1(*,*,i+1))])
     endif else fmax(i)=max([max(w0(*,*,i)),max(w1(*,*,i))])
  endfor
  plotmode='contfilllognoaxis'
  bottomline=0
  !p.charsize=1.5
  plottitles_file=['Model AIA:94; Model AIA:131; Model AIA:171; Model AIA:193; Model AIA:211; Model AIA:335','Obs AIA:94; Obs AIA:131; Obs AIA:171; Obs AIA:193; Obs AIA:211; Obs AIA:335']
  plot_spacex=1
  plot_spacey=1.5
  printf, unitlog,'aia94: max(obs), max(swmf) = ',$
          max(w1(*,*,0)), max(w0(*,*,0))
  printf, unitlog,'aia94: min(obs), min(swmf) = ', $
          min(w1(*,*,0)), min(w0(*,*,0))
  printf, unitlog,'aia171: max(obs), max(swmf) = ',$
          max(w1(*,*,2)), max(w0(*,*,2))
  printf, unitlog,'aia171: min(obs), min(swmf) = ',$
          min(w1(*,*,2)), min(w0(*,*,2))
  printf, unitlog,'aia193: max(obs), max(swmf) = ',$
          max(w1(*,*,3)), max(w0(*,*,3))
  printf, unitlog,'aia193: min(obs), min(swmf) = ',$
          min(w1(*,*,3)), min(w0(*,*,3))
  printf, unitlog,'aia131: max(obs), max(swmf) = ',$
          max(w1(*,*,1)), max(w0(*,*,1))
  printf, unitlog,'aia131: min(obs), min(swmf) =',$
          min(w1(*,*,1)), min(w0(*,*,1))
  printf, unitlog,'aia211: max(obs), max(swmf) = ',$
          max(w1(*,*,4)), max(w0(*,*,4))
  printf, unitlog,'aia211: min(obs), min(swmf) = ',$
          min(w1(*,*,4)), min(w0(*,*,4))
  printf, unitlog,'aia304: max(obs), max(swmf) = ',$
          max(w1(*,*,5)), max(w0(*,*,5))
  printf, unitlog,'aia304: min(obs), min(swmf) = ',$
          min(w1(*,*,5)), min(w0(*,*,5))
  printf, unitlog,'aia335: max(obs), max(swmf) = ',$
          max(w1(*,*,6)), max(w0(*,*,6))
  printf, unitlog,'aia335: min(obs), min(swmf) = ',$
          min(w1(*,*,6)), min(w0(*,*,6))

  set_device,file_plot+'_IDL.eps'
  device,xsize=20,ysize=26
  animate_data
  close_device,/pdf
end

;--------------------------------------------------------------------

pro get_insitu_data, start_time, end_time, TypeData, u_obs,     $
                     n_obs, tem_obs, mag_obs, time_obs, br_obs, $
                     DoContainData=DoContainData

  DoContainData = 1

  case TypeData of
     'OMNI': begin
        obs_data=spdfgetdata('OMNI_COHO1HR_MERGED_MAG_PLASMA', $
                             ['ABS_B' , 'V', 'N','T','BR'],    $
                             [start_time, end_time])
        
        if(not isa(obs_data)) then begin
           print, ' Could not obtain OMNI data.'
           RETURN
        endif
        u_obs    = obs_data.v.dat
        n_obs    = obs_data.n.dat
        tem_obs  = obs_data.t.dat
        mag_obs  = obs_data.abs_b.dat
        br_obs   = obs_data.BR.dat
        time_obs = obs_data.epoch.dat
     end

     'solo': begin
        obs_data=spdfgetdata('SOLO_COHO1HR_MERGED_MAG_PLASMA',      $
                             ['B' , 'ProtonSpeed', 'protonDensity', $
                              'protonTemp','BR'],[start_time, end_time])

        if(not isa(obs_data)) then begin
           print, ' Could not obtain Solar Orbiter data.'
           RETURN
        endif

        u_obs    = obs_data.ProtonSpeed.dat
        n_obs    = obs_data.protonDensity.dat
        tem_obs  = obs_data.protonTemp.dat
        mag_obs  = obs_data.B.dat
        br_obs   = obs_data.BR.dat
        time_obs = obs_data.epoch.dat
     end

     'psp': begin
        obs_data=spdfgetdata('PSP_COHO1HR_MERGED_MAG_PLASMA',          $
                             ['B' , 'ProtonSpeed', 'protonDensity',    $
                              'protonTemp','BR'],[start_time, end_time])

        if(not isa(obs_data)) then begin
           print, ' Could not obtain Parker Solar Probe data.'
           RETURN
        endif

        u_obs    = obs_data.ProtonSpeed.dat
        n_obs    = obs_data.protonDensity.dat
        tem_obs  = obs_data.protonTemp.dat
        mag_obs  = obs_data.B.dat
        br_obs   = obs_data.BR.dat
        time_obs = obs_data.epoch.dat
     end

     'Stereo A': begin
        obs_data=spdfgetdata('STA_COHO1HR_MERGED_MAG_PLASMA',       $
                             ['B' , 'plasmaSpeed', 'plasmaDensity', $
                              'plasmaTemp','BR'],[start_time, end_time])

        if(not isa(obs_data)) then begin
           DoContainData = 0
           print, ' Could not obtain Stereo A data.'
           RETURN
        endif

        u_obs    = obs_data.plasmaSpeed.dat
        n_obs    = obs_data.plasmaDensity.dat
        tem_obs  = obs_data.plasmaTemp.dat
        mag_obs  = obs_data.B.dat
        br_obs   = obs_data.BR.dat
        time_obs = obs_data.epoch.dat
     end

     'Stereo B': begin
        obs_data=spdfgetdata('STB_COHO1HR_MERGED_MAG_PLASMA',       $
                             ['B' , 'plasmaSpeed', 'plasmaDensity', $
                              'plasmaTemp','BR'],[start_time, end_time])

        if(not isa(obs_data)) then begin
           DoContainData = 0
           print, ' Could not obtain Stereo B data.'
           RETURN
        endif

        u_obs    = obs_data.plasmaSpeed.dat
        n_obs    = obs_data.plasmaDensity.dat
        tem_obs  = obs_data.plasmaTemp.dat
        mag_obs  = obs_data.B.dat
        br_obs   = obs_data.BR.dat
        time_obs = obs_data.epoch.dat
     end

     'none': begin
        DoContainData = 0
        print, ' case: unknown TypeData.'
        RETURN
     end
  endcase
end

;--------------------------------------------------------------------

pro get_CME_interval, filename, start_time_I, end_time_I

  openr, lun_local, filename, /get_lun

  line  = ''
  nLine = 0

  ;; maximum 10000 CMEs
  start_time_I = strarr(10000)
  end_time_I   = strarr(10000)

  while not eof(lun_local) do begin
     readf, lun_local, line
     if line ne 'start_time,end_time' then begin
        times = strsplit(line,',',/extract)
        start_time_I(nLine) = times(0)
        end_time_I(nLine)   = times(1)
        nLine += 1
     endif
  endwhile

  close, lun_local & free_lun, lun_local

  start_time_I = start_time_I(0:nLine-1)
  end_time_I   = end_time_I(0:nLine-1)

  ;; use the IDL format
  start_time_I = start_time_I.replace(' ','T')
  end_time_I   = end_time_I.replace(' ','T')
end

;--------------------------------------------------------------------

pro plot_CME_interval, start_time_CME_epoch_I, end_time_CME_epoch_I, index_cme, $
                       start_time_epoch, end_time_epoch, ymin, ymax

  xmax_local = (end_time_epoch-start_time_epoch)/1000

  for i=0,n_elements(index_cme)-1 do begin
     iLocal = index_cme(i)
     if iLocal ge 0 then begin
        start_time_CME = start_time_CME_epoch_I(iLocal)
        end_time_CME   = end_time_CME_epoch_I(iLocal)

        x1_cme = max([(start_time_CME - start_time_epoch)/1000, 0])
        x2_cme = min([(end_time_CME   - start_time_epoch)/1000, xmax_local])

        POLYFILL, [x1_cme,x2_cme,x2_cme,x1_cme],[ymin,ymin,ymax,ymax],/data,color=87
     endif
  endfor
end
