;  Copyright (C) 2002 Regents of the University of Michigan,
;  portions used with permission 
;  For more information, see http://csem.engin.umich.edu/tools/swmf

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function ask, what, orig_ans, set_orig = set_orig

  if n_elements(orig_ans) eq 0 then orig_ans = ''

  nAnswers = n_elements(orig_ans)

  if (nAnswers eq 1) then begin

      answer = ''
      read, 'Enter '+what+' ['+orig_ans+'] : ', answer
      if strlen(answer) eq 0 then answer = orig_ans
      if n_elements(set_orig) gt 0 then orig_ans = answer

  endif else begin

      answer = strarr(nAnswers)

      TempAnswer = ''
      for i = 0, nAnswers-1 do begin
          read, 'Enter '+what+' '+tostr(i)+' ['+orig_ans(i)+'] : ', TempAnswer
          if strlen(TempAnswer) eq 0 then TempAnswer = orig_ans(i)
          Answer(i) = TempAnswer
      endfor

  endelse

  return, answer

end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function mm, array
;  computes min and max of an array and returns them in an array
return, [min(array),max(array)]
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function chopr, svalue, n
  if strlen(svalue) lt n then n = strlen(svalue)
  return, strmid(svalue, strlen(svalue)-n,n)
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function chopl, svalue, n
  if strlen(svalue) lt n then n = strlen(svalue)
  return, strmid(svalue, 0, n)
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function mklower, string

  temp = byte(string)

  loc = where((temp ge 65) and (temp le 90), count)

  if count ne 0 then temp(loc) = temp(loc)+32

  return, string(temp)

end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function jday, year, mon, day
  if mon lt 2 then return, day
  dayofmon = [31,28,31,30,31,30,31,31,30,31,30,31]
  if year mod 4 eq 0 and (year mod 100 ne 0 or year mod 400 eq 0) $
    then dayofmon(1) = dayofmon(1) + 1
  return, total(dayofmon(0:mon-2)) + day
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
function tostr, value
  return, strcompress(string(long(value)),/remove_all)
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
function convert_time, currenttime, longitude

if longitude lt 0 then longitude = longitude + 360
longitude = longitude * !dtor
Mars_RP = 88775.0 ;seconds
Mars_hoursperday = Mars_RP/3600.0
MarsVernalTime = [1998,7,14,16,0,0]
Mars_DPY = 670.0
Mars_SPY = Mars_DPY*Mars_RP

;Earth_RP = 24.0*3600.0
;Earth_hoursperday =Earth_RP/3600.0
;EarthVernalTime = [1999,3,21,0,0,0]
;Earth_DPY = 325.25
;Earth_SPY = Earth_DPY * Earth_RP

c_a_to_r, MarsVernalTime,mVernalTime

dtime = currenttime - mvernaltime
iday = fix(dtime/Mars_RP)
utime = (dtime/Mars_RP - iday) * Mars_RP

localtime = (UTime/3600.0 + $
       Longitude * Mars_HoursPerDay / (2*!Pi)) mod Mars_HoursPerDay

return, localtime
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
function gettok,st,char, exact=exact
;+
; NAME:
;	GETTOK                                    
; PURPOSE:
;	Retrieve the first part of a (vector) string up to a specified character
; EXPLANATION:
;	GET TOKen - Retrieve first part of string until the character char 
;	is encountered.   
;
; CALLING SEQUENCE:
;	token = gettok( st, char, [ /EXACT ] )
;
; INPUT:
;	char - character separating tokens, scalar string
;
; INPUT-OUTPUT:
;	st - string to get token from (on output token is removed),
;            scalar or vector
;
; OUTPUT:
;	token - extracted string value is returned, same dimensions as st
; OPTIONAL INPUT KEYWORD:
;       /EXACT -  The default behaviour of GETTOK is to remove any leading 
;              blanks and (if the token is a blank) convert tabs to blanks.    
;              Set the /EXACT keyword to skip these steps and leave the 
;              input string unchanged before searching for the  character 
;              tokens. 
;
; EXAMPLE:
;	If ST is ['abc=999','x=3.4234'] then gettok(ST,'=') would return
;	['abc','x'] and ST would be left as ['999','3.4234'] 
;
; PROCEDURE CALLS:
;       REPCHR()
; HISTORY
;	version 1  by D. Lindler APR,86
;	Remove leading blanks    W. Landsman (from JKF)    Aug. 1991
;	Converted to IDL V5.0   W. Landsman   September 1997
;       V5.3 version, accept vector input   W. Landsman February 2000
;       Slightly faster implementation  W. Landsman   February 2001
;       Added EXACT keyword  W. Landsman March 2004
;       Assume since V5.4, Use COMPLEMENT keyword to WHERE W. Landsman Apr 2006
;-
;----------------------------------------------------------------------
  On_error,2                           ;Return to caller
  compile_opt idl2

   if N_params() LT 2 then begin
       print,'Syntax - token = gettok( st, char, [ /EXACT ] )'
       return,-1
   endif

; if char is a blank treat tabs as blanks

 if not keyword_set(exact) then begin
    st = strtrim(st,1)              ;Remove leading blanks and tabs
    if char EQ ' ' then begin 
       tab = string(9b)                 
       if max(strpos(st,tab)) GE 0 then st = repchr(st,tab,' ')
    endif
  endif
  token = st

; find character in string

  pos = strpos(st,char)
  test = pos EQ -1
  bad = where(test, Nbad, Complement = good, Ncomplement=Ngood)
  if Nbad GT 0 then st[bad] = ''
 
; extract token
 if Ngood GT 0 then begin
    stg = st[good]
    pos = reform( pos[good], 1, Ngood )
    token[good] = strmid(stg,0,pos)
    st[good] = strmid(stg,pos+1)
 endif

;  Return the result.

return,token
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
function date_conv,date,type
;+
; NAME:
;     DATE_CONV
; PURPOSE:
;     Procedure to perform conversion of dates to one of three possible formats.
;
; EXPLANATION:
;     The following date formats are allowed
;
;       format 1: real*8 scalar encoded as:
;               year*1000 + day + hour/24. + min/24./60 + sec/24./60/60
;               where day is the day of year (1 to 366)
;       format 2: Vector encoded as:
;               date[0] = year (eg. 2005)
;               date[1] = day of year (1 to 366)
;               date[2] = hour
;               date[3] = minute
;               date[4] = second
;       format 3: string (ascii text) encoded as
;               DD-MON-YEAR HH:MM:SS.SS
;               (eg.  14-JUL-2005 15:25:44.23)
;            OR
;               YYYY-MM-DD HH:MM:SS.SS  (ISO standard)
;               (eg.  1987-07-14 15:25:44.23 or 1987-07-14T15:25:44.23)
;                   
;       format 4: three element vector giving spacecraft time words
;       from a Hubble Space Telescope (HST) telemetry packet.   Based on
;       total number of secs since midnight, JAN. 1, 1979
;
; CALLING SEQUENCE
;       results = DATE_CONV( DATE, TYPE )
;
; INPUTS:
;       DATE - input date in one of the three possible formats.
;       TYPE - type of output format desired.  If not supplied then
;               format 3 (real*8 scalar) is used.
;                       valid values:
;                       'REAL'  - format 1
;                       'VECTOR' - format 2
;                       'STRING' - format 3
;                       'FITS' - YYYY-MM-DDTHH:MM:SS.SS'
;               TYPE can be abbreviated to the single character strings 'R',
;               'V', 'S' and 'F'.
;               Nobody wants to convert TO spacecraft time (I hope!)
; OUTPUTS:
;       The converted date is returned as the function value.
;
; EXAMPLES:
;       IDL> print,date_conv('2006-03-13 19:58:00.00'),f='(f15.5)' 
;             2006072.83194 
;       IDL> print,date_conv( 2006072.8319444d,'F')
;             2006-03-13T19:58:00.00
;       IDL> print,date_conv( 2006072.8319444d,'V')
;             2006.00      72.0000      19.0000      57.0000      59.9962
;
; HISTORY:
;      version 1  D. Lindler  July, 1987
;      adapted for IDL version 2  J. Isensee  May, 1990
;      Made year 2000 compliant; allow ISO format input  jls/acc Oct 1998
;      DJL/ACC Jan 1998, Modified to work with dates such as 6-JAN-1996 where
;               day of month has only one digit.
;      DJL, Nov. 2000, Added input/output format YYYY-MM-DDTHH:MM:SS.SS
;      Replace spaces with '0' in output FITS format  W.Landsman April 2006
;-
;-------------------------------------------------------------
;
compile_opt idl2
; data declaration
;
days = [0,31,28,31,30,31,30,31,31,30,31,30,31]
months = ['   ','JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT',$
        'NOV','DEC']
;
; set default type if not supplied
;
if N_params() lt 2 then type = 'REAL'
;
; Determine type of input supplied
;
s = size(date) & ndim = s[0] & datatype = s[ndim+1]
if ndim gt 0 then begin                 ;vector?
        if ndim gt 1 then goto,notvalid
        if (s[1] ne 5) and (s[1] ne 3) then goto,notvalid
        if (s[1] eq 5) then form = 2 else form = 4
   end else begin                       ;scalar input
        if datatype eq 0 then goto,notvalid
        if datatype eq 7 then form = 3 $        ;string
                         else form = 1  ;numeric scalar
end
;
;      -----------------------------------
;
;*** convert input to year,day,hour,minute,second
;
;      -----------------------------------
case form of

        1: begin                                        ;real scalar
                idate = long(date)
                year = long(idate/1000)
;
; if year is only 2 digits, assume 1900
;
                if year lt 100 then begin
                   message,/WARN, $
                     'Warning: Year specified is only 2 digits, assuming 19xx'
                   year=1900+year
                   idate=1900000+idate
                   date=1900000.+date
                end
;
                day = idate - year*1000
                fdate = date-idate
                fdate = fdate*24.
                hour = fix(fdate)
                fdate = (fdate-hour)*60.0
                minute = fix(fdate)
                sec = float((fdate-minute)*60.0)
           end

        2: begin                                        ;vector
                year = fix(date[0])
;
; if year is only 2 digits, assume 1900
;
                if year lt 100 then begin
                   message,/WARN, $
                    'Warning: Year specified is only 2 digits, assuming 19xx'
                   year=1900+year
                end
;
                day = fix(date[1])
                hour = fix(date[2])
                minute = fix(date[3])
                sec = float(date[4])
           end

        3: begin                                        ;string
                temp = date
;
; check for old type of date, DD-MMM-YYYY
;
                if strpos(temp,'-') le 2 then begin
                  day_of_month = fix(gettok(temp,'-'))
                  month_name = gettok(temp,'-')
                  year = fix(gettok(temp,' '))
                  hour = fix(gettok(temp,':'))
                  minute = fix(gettok(temp,':'))
                  sec = float(strtrim(strmid(temp,0,5)))
;
; determine month number from month name
;
                  month_name = strupcase(month_name)
                  for mon = 1,12 do begin
                        if month_name eq months[mon] then goto,found
                  end
                  message,'Invalid month name specified'
                  
;
; check for new type of date, ISO: YYYY-MM-DD
;
                end else if strpos(temp,'-') eq 4 then begin
                  year = fix(gettok(temp,'-'))
                  month_name = gettok(temp,'-')
                  mon=month_name
                  day_of_month=gettok(temp,' ')
                  if strlen(temp) eq 0 then begin
                        dtmp=gettok(day_of_month,'T')
                        temp=day_of_month
                        day_of_month=dtmp
                  end
                  day_of_month=fix(day_of_month)
                  hour = fix(gettok(temp,':'))
                  minute = fix(gettok(temp,':'))
                  sec = float(strtrim(strmid(temp,0,5)))
                end else goto, notvalid
              found:
;
; if year is only 2 digits, assume 1900
;
                if year lt 100 then begin
                   message,/WARN, $
                     'Warning: Year specified is only 2 digits, assuming 19xx'
                   year=1900+year
                end
;
;
;            convert to day of year from month/day_of_month
;
;            correction for leap years
;
;               if (fix(year) mod 4) eq 0 then days(2) = 29     ;add one to february
                lpyr = ((year mod 4) eq 0) and ((year mod 100) ne 0) $
                        or ((year mod 400) eq 0)
                if lpyr eq 1 then days[2] = 29 ; if leap year, add day to Feb.
;
;
;            compute day of year
;
                  day = fix(total(days[0:mon-1])+day_of_month)
           end

        4 : begin                       ;spacecraft time
                SC = DOUBLE(date)
                SC = SC + (SC LT 0.0)*65536.    ;Get rid of neg. numbers 
;
;            Determine total number of secs since midnight, JAN. 1, 1979
;
                SECS = SC[2]/64 + SC[1]*1024 + SC[0]*1024*65536.
                SECS = SECS/8192.0D0            ;Convert from spacecraft units 
;
;            Determine number of years 
;
                MINS = SECS/60.
                HOURS = MINS/60.
                TOTDAYS = HOURS/24.
                YEARS = TOTDAYS/365.
                YEARS = FIX(YEARS)
;
;            Compute number of leap years past 
;
                LEAPYEARS = (YEARS+2)/4
;
;           Compute day of year 
;
                DAY = FIX(TOTDAYS-YEARS*365.-LEAPYEARS)
;
;           Correct for case of being right at end of leapyear
;
                IF DAY LT 0 THEN BEGIN
                  DAY = DAY+366
                  LEAPYEARS = LEAPYEARS-1
                  YEARS = YEARS-1
                END
;
;            COMPUTE HOUR OF DAY
;
                TOTDAYS = YEARS*365.+DAY+LEAPYEARS
                HOUR = FIX(HOURS - 24*TOTDAYS)
                TOTHOURS = TOTDAYS*24+HOUR
;
;            COMPUTE MINUTE
;
                MINUTE = FIX(MINS-TOTHOURS*60)
                TOTMIN = TOTHOURS*60+MINUTE
;
;            COMPUTE SEC
;
                SEC = SECS-TOTMIN*60
;
;            COMPUTE ACTUAL YEAR
;
                YEAR = YEARS+79
;
; if year is only 2 digits, assume 1900
;
                if year lt 100 then begin
                   message, /CON, $ 
                     'Warning: Year specified is only 2 digits, assuming 19xx'
                   year=1900+year
                end
;
;
;            START DAY AT ONE AND NOT ZERO
;
                DAY=DAY+1
           END
ENDCASE
;
;            correction for leap years
;
        if form ne 3 then begin         ;Was it already done?
           lpyr = ((year mod 4) eq 0) and ((year mod 100) ne 0) $
                or ((year mod 400) eq 0)
           if lpyr eq 1 then days[2] = 29 ; if leap year, add day to Feb.
        end
;
;            check for valid day
;
        if (day lt 1) or (day gt total(days)) then $
            message,'ERROR -- There are only ' + strtrim(fix(total(days)),2) + $
	         ' days  in year '+strtrim(year,2)

;
;            find month which day occurs
;
        day_of_month = day
        month_num = 1
        while day_of_month gt days[month_num] do begin
               day_of_month = day_of_month - days[month_num]
               month_num = month_num+1
        end
;           ---------------------------------------
;
;   *****       Now convert to output format
;
;           ---------------------------------------
;
; is type a string
;
s = size(type)
if (s[0] ne 0) or (s[1] ne 7) then $
        message,'ERROR - Output type specification must be a string'
;
case strmid(strupcase(type),0,1) of

        'V' : begin                             ;vector output
                out = fltarr(5)
                out[0] = year
                out[1] = day
                out[2] = hour
                out[3] = minute
                out[4] = sec
             end
 
        'R' : begin                             ;floating point scalar
;               if year gt 1900 then year = year-1900
                out = sec/24.0d0/60./60. + minute/24.0d0/60. + hour/24.0d0 $
                        +  day + year*1000d0
              end

        'S' : begin                             ;string output 

                month_name = months[month_num]
;
;            encode into ascii_date
;
                out = string(day_of_month,'(i2)') +'-'+ month_name +'-' + $
                        string(year,'(i4)') + ' '+ $
                        string(hour,'(i2.2)') +':'+ $
                        strmid(string(minute+100,'(i3)'),1,2) + ':'+ $
                        strmid(string(sec+100,'(f6.2)'),1,5)
           end
        'F' : begin
               xsec = strmid(string(sec+100,'(f6.2)'),1,5)
               if xsec EQ '60.00' then begin
                     minute = minute+1
                     xsec = '00.00'
                endif
                xminute =   string(minute,'(i2.2)')
                if xminute EQ '60' then begin
                       hour = hour+1
                       xminute = '00'                  
                endif          
                out = string(year,'(i4)')+'-'+string(month_num,'(I2.2)')+'-'+ $
                        string(day_of_month,'(i2.2)')+'T' + $
                        string(hour,'(i2.2)') +  ':' +xminute + ':'+ xsec
                        
              end
        else: begin                     ;invalid type specified
                print,'DATE_CONV-- Invalid output type specified'
                print,' It must be ''REAL'', ''STRING'', or ''VECTOR'''
                return,-1
              end
endcase
return,out
;
; invalid input date error section
;
notvalid:
message,'Invalid input date specified',/CON
return, -1
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
function tostrf,value
  return, strcompress(string(float(value)),/remove_all)
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PRO compute_axis, times, placement, btr, etr, curvar, xlab, xtl,	$
		basetime, moncheck, nl, ticktime, nminor

  ticktime = dblarr(20)
  xlab = strarr(20)
  timedum = intarr(6)

  nl = 1
  skip = 1

  dayofmon = [31,28,31,30,31,30,31,31,30,31,30,31]

  pc = placement(curvar,0)

  if (times(pc,0,0) ge 65) then begin
    byear = 1900+times(pc,0,0)
    eyear = 1900+times(pc,1,0)
  endif else begin
    byear = 2000+times(pc,0,0)
    eyear = 2000+times(pc,1,0)
  endelse

  bmonth = times(pc,0,1)
  emonth = times(pc,1,1)
  bday = times(pc,0,2)
  eday = times(pc,1,2)
  bhour = times(pc,0,3)
  ehour = times(pc,1,3)
  bminute = times(pc,0,4)
  eminute = times(pc,1,4)
  bsecond = times(pc,0,5)
  esecond = times(pc,1,5)

  sbd = '0'+tostr(bday)
  sbd = strmid(sbd,strlen(sbd)-2,2)
  sed = '0'+tostr(eday)
  sed = strmid(sed,strlen(sed)-2,2)
  sbh = '0'+tostr(bhour)
  sbh = strmid(sbh,strlen(sbh)-2,2)
  seh = '0'+tostr(ehour)
  seh = strmid(seh,strlen(seh)-2,2)
  sbm = '0'+tostr(bminute)
  sbm = strmid(sbm,strlen(sbm)-2,2)
  sem = '0'+tostr(eminute)
  sem = strmid(sem,strlen(sem)-2,2)

  begt = times(pc,0,*)
  btr = double(0.0)
  c_a_to_r, begt, btr
  btr = btr - basetime(pc)

  endt = times(pc,1,*)
  etr = double(0.0)
  c_a_to_r, endt, etr
  etr = etr - basetime(pc)

  dt = etr - btr

  if (byear mod 4 eq 0) and (bmonth eq 2) then 		$
    dayofmon(1) = dayofmon(1)+1

  secperyear = 365.0*24.0*60.0*60.0
  secpermon  = float(dayofmon(bmonth-1))*24.0*60.0*60.0
  secperday  = 24.0*60.0*60.0
  secperhour = 60.0*60.0
  secpermin  = 60.0

  if dt ge 2.0*secperyear then begin

    step = (eyear - byear)/5 + 1
    step = 1
    nl = 0

    for j = byear, eyear+step, step do begin

      xlab(j-byear) = tostr(j)
      timedum = [j,1,0,0,0,0]
      c_a_to_r, timedum, dum
      ticktime(j-byear) = dum - basetime(pc)
      nl = nl + 1

    endfor

    xtl = 'UT Years'
    nminor = 6

  endif else begin

    if dt ge 2.0*secpermon then begin

      nmon = fix(dt/secpermon) - 1
      step = nmon/5 + 1
      nl = 0
      if byear eq eyear then exten = 0 else exten = 1

      for j=0, nmon+step, step do begin

	cmonth = bmonth + j
	cyear = byear
	if cmonth gt 12 then begin
	  if cmonth mod 12 ne 0 then begin
	    cyear = cyear + cmonth / 12
	    cmonth = cmonth mod 12
	  endif else begin
	    cyear = cyear + cmonth / 12 - 1
	    cmonth = 12
	  endelse
	endif

	xlab(nl) = strmid(moncheck,(cmonth-1)*3,3)
	if exten then xlab(nl) = xlab(nl) + ', ' + tostr(cyear)
	timedum = [cyear,cmonth,0,0,0,0]
	c_a_to_r, timedum, dum
	ticktime(nl) = dum - basetime(pc)
	nl = nl + 1

      endfor

      xlab(nl) = strmid(moncheck,(emonth-1)*3,3)
      nminor = 6
      if exten then xtl = 'UT Time'		$
      else xtl = tostr(byear)+' Universal Time'

    endif else begin

      if dt ge 2.0*secperday then begin

	nday = fix(dt/secperday) - 1
	step = nday/5 + 1
	nl = 0
	if bmonth eq emonth then exmon = 0 else exmon = 1
	if byear eq eyear then exyear = 0 else exyear = 1

	for j =	0, nday+step, step do begin

	  cday = bday + j
	  cmonth = bmonth
	  cyear = byear

; since we blindly incremented cday, we have to make sure that the right
; month and day are used. This is very easy to do, since the convering
; array to real subroutine does not care about too many days in a month - 
; so [91,2,29,0,0,0] = [91,3,1,0,0,0] ect.
; when we convert it back to an array, it is converted back normally.

	  timedum = [cyear,cmonth,cday,0,0,0]
	  c_a_to_r, timedum, dum
	  ticktime(nl) = dum - basetime(pc) 
	  c_r_to_a, timedum, dum

	  cmonth = timedum(1)
	  cday = timedum(2)

	  sday = '0'+tostr(cday)
	  sday = strmid(sday,strlen(sday)-2,2)
	  if exmon then 					$
	    xlab(nl) = strmid(moncheck,(cmonth-1)*3,3) + 	$
			' ' + sday				$
	  else xlab(nl) = sday

	  nl = nl + 1

	endfor

	nminor = step

	if exmon then begin

	  if exyear then					$
	    xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		  sbd + ', ' + tostr(byear) + ' to ' +	$
		  strmid(moncheck,(emonth-1)*3,3) + ' ' + 	$
		  sed + ', ' + tostr(eyear) + 		$
		  ' Universal Time'				$
	  else							$
	    xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		  sbd + ' to ' +			$
		  strmid(moncheck,(emonth-1)*3,3) + ' ' + 	$
		  sed + ', ' + tostr(eyear) + 		$
		  ' Universal Time'

	endif else						$
	  xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		sbd + ' to ' +				$
		sed + ', ' + tostr(eyear) + 		$
		' Universal Time'

      endif else begin

	if dt ge 2.0*secperhour then begin

	  nhour = fix(dt/secperhour) - 1
	  step = nhour/5 + 1
	  nl = 0
	  if bday eq eday then exday = 0 else exday = 1
	  if bmonth eq emonth then exmon = 0 else exmon = 1
	  if byear eq eyear then exyear = 0 else exyear = 1

	  for j = 0, nhour+step, step do begin

	    chour = bhour + j
	    cday = bday
	    cmonth = bmonth
	    cyear = byear

; since we blindly incremented cday, we have to make sure that the right
; month and day are used. This is very easy to do, since the convering
; array to real subroutine does not care about too many days in a month - 
; so [91,2,28,25,0,0] = [91,3,1,1,0,0] ect.
; when we convert it back to an array, it is converted back normally.

	    timedum = [cyear,cmonth,cday,chour,0,0]
	    c_a_to_r, timedum, dum
	    ticktime(nl) = dum - basetime(pc) 
	    c_r_to_a, timedum, dum

	    chour = timedum(3)

	    xlab(nl) = '0'+tostr(chour)
	    xlab(nl) = strmid(xlab(nl),strlen(xlab(nl))-2,2)

	    nl = nl + 1

	  endfor

	  nminor = step

	  if exday and (not exmon) and (not exyear) then 	$
	    if (bhour+bminute+bsecond eq 0) and			$
	       (ehour+eminute+esecond eq 0) and			$
               (bday eq eday-1) then exday = 0

	  if not exday then begin
	    xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		  sbd + ', ' + tostr(byear) + 		$
		  ' UT Hours'
	  endif else begin
	    if not exmon then begin
	      xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		    sbd + ' to ' +			$
		    sed + ', ' + tostr(eyear) + 	$
		    ' UT Hours'
	    endif else begin
	      if not exyear then begin
		xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		      sbd + ' to ' +			$
		      strmid(moncheck,(emonth-1)*3,3) + ' ' + 	$
		      sed + ', ' + tostr(eyear) + 	$
		      ' UT Hours'
	      endif else begin
		xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		      sbd + ', ' + tostr(byear) + 	$
		      ' to ' +					$
		      strmid(moncheck,(emonth-1)*3,3) + ' ' + 	$
		      sed + ', ' + tostr(eyear) + 	$
		      ' UT Hours'
	      endelse
	    endelse
	  endelse

	endif else begin

	  if dt ge 2.0*secpermin then do_min = 1 else do_min = 0

	  if do_min then ntotal = fix(dt/secpermin) - 1		$
	  else ntotal = fix(dt) - 1

	  step = ntotal/5 + 1

	  nl = 0
	  if bminute eq eminute then exmin = 0 else exmin = 1
	  if bhour eq ehour then exhour = 0 else exhour = 1
	  if bday eq eday then exday = 0 else exday = 1
	  if bmonth eq emonth then exmon = 0 else exmon = 1
	  if byear eq eyear then exyear = 0 else exyear = 1

	  for j = 0, ntotal+step, step do begin

	    if do_min then begin
	      csecond = 0
	      cminute = bminute + j
	    endif else begin
	      csecond = bsecond + j
	      cminute = bminute
	    endelse
	    chour = bhour
	    cday = bday
	    cmonth = bmonth
	    cyear = byear

; since we blindly incremented cday, we have to make sure that the right
; month and day are used. This is very easy to do, since the convering
; array to real subroutine does not care about too many days in a month - 
; so [91,2,28,25,0,0] = [91,3,1,1,0,0] ect.
; when we convert it back to an array, it is converted back normally.

	    timedum = [cyear,cmonth,cday,chour,cminute,csecond]
	    c_a_to_r, timedum, dum
	    ticktime(nl) = dum - basetime(pc) 
	    c_r_to_a, timedum, dum

	    if do_min then begin
              ctime = timedum(4) 
	      xlab(nl) = chopr('0'+tostr(timedum(3)),2)+chopr('0'+tostr(timedum(4)),2)+' UT'
            endif else begin
              ctime = timedum(5)
	    endelse

	    nl = nl + 1

	  endfor

	  nminor = step

	  if not exhour then begin
	      xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		    sbd + ', ' + tostr(byear) + 	$
		    ' ' + sbh + ':' + sbm
	  endif else begin
	    if not exday then begin
	        xtl = strmid(moncheck,(bmonth-1)*3,3) + ' ' + 	$
		      sbd + ', ' + tostr(byear) + 	$
		      ' ' + sbh + ':' + sbm +	$
		      ' to ' + seh + ':' + 		$
		      sem
	    endif else begin
	      if not exmon then begin
		  xtl = strmid(moncheck,(bmonth-1)*3,3) + 	$
			' ' + sbd + ' ' + 		$
			sbh + ':' + sbm +	$
			' to ' + 				$
			strmid(moncheck,(emonth-1)*3,3) + ' ' + $
			sed + ' ' + 		$
			seh + ':' + sem +	$
			', ' + tostr(eyear)
	      endif else begin
		if not exyear then begin
		    xtl = strmid(moncheck,(bmonth-1)*3,3) + 	$
			' ' + sbd + ' ' +		$
			sbh + ':' + sbm +	$
			' to ' + 				$
			strmid(moncheck,(emonth-1)*3,3) + ' ' + $
			sed + ' ' +			$
			seh + ':' + sem +	$
			', ' + tostr(eyear)
		endif else begin
		    xtl = strmid(moncheck,(bmonth-1)*3,3) + 	$
		      ' ' + sbd + ' ' +			$
		      sbh + ':' + sbm +	$
		      ', ' + tostr(byear) + 			$
		      ' to ' +					$
		      strmid(moncheck,(emonth-1)*3,3) + ' ' + 	$
		      sed + ' ' +			$
		      seh + ':' + sem +	$
		      ', ' + tostr(eyear)
		endelse
	      endelse
	    endelse
	  endelse
          if do_min then xtl = xtl + ' UT Minutes'		$
	  else xtl = xtl + ' UT Seconds'

	endelse

      endelse

    endelse

  endelse

  nl = nl - 1

;  loc = where(ticktime gt btr and ticktime lt etr, count)
;  if count gt 0 then begin
;    ticktime = ticktime(loc)
;    xlab = xlab(loc)
;    nl = count
;  endif

  if ticktime(0) lt btr then begin

    xlab = xlab(1:nl+1)
    ticktime = ticktime(1:nl)
    nl = nl - 1

  endif

  if (ticktime(nl) gt etr) or (ticktime(nl) lt btr) then begin

    xlab = [xlab(0:nl-1), xlab(nl+1)]
    ticktime = ticktime(0:nl-1)
    nl = nl - 1

  endif

  if nminor eq 1 then nminor = 4

  return

end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
pro time_axis, stime, etime, s_time_range, e_time_range, 	$
	xtickname, xtitle, xtickvalue, xminor, xtickn

  times = intarr(1,2,6)

  if n_elements(stime) eq 6 then begin
    times(0,0,*) = stime
    times(0,1,*) = etime
  endif else begin
    c_r_to_a, itime, stime
    times(0,0,*) = itime
    c_r_to_a, itime, etime
    times(0,1,*) = itime
  endelse

  if times(0,0,0) gt 1900 then times(0,0,0) = times(0,0,0) - 1900
  if times(0,0,0) ge 100 then times(0,0,0) = times(0,0,0) - 100
  if times(0,1,0) gt 1900 then times(0,1,0) = times(0,1,0) - 1900
  if times(0,1,0) ge 100 then times(0,1,0) = times(0,1,0) - 100

  placement = intarr(1,1)*0
  curvar = 0

  c_a_to_r, times(0,0,*), bt

  basetime = dblarr(1)
  basetime(0) = bt

  moncheck = "JanFebMarAprMayJunJulAugSepOctNovDec"

  compute_axis, times, placement, s_time_range, e_time_range,	$
	curvar, xtickname, xtitle, basetime, moncheck,		$
	xtickn, xtickvalue, xminor

  return

end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
pro zsun,date,time,lat,lon,zenith,azimuth,solfac,sunrise=sunrise, $
           sunset=sunset,local=local,latsun=latsun,lonsun=lonsun
;+
; ROUTINE:      zensun
;
; PURPOSE:      Compute solar position information as a function of
;               geographic coordinates, date and time.
;               
; USEAGE:       zensun,day,time,lat,lon,zenith,azimuth,solfac,sunrise,sunset,
;                  local=local
;
; INPUT:
;   day         'YYYY-MM-DD'
;
;   time        Universal Time in hours (scalar or vector) 
;
;   lat         geographic latitude of point on earth's surface (degrees)
;
;   lon         geographic longitude of point on earth's surface (degrees)
;
; OUTPUT:
;
;   zenith      solar zenith angle (degrees)
;
;   azimuth     solar azimuth  (degrees) 
;               Azimuth is measured clockwise from due north 
;
;   solfac      Solar flux multiplier.  SOLFAC=cosine(ZENITH)/RSUN^2
;               where rsun is the current earth-sun distance in
;               astronomical units.
;
;               NOTE: SOLFAC is negative when the sun is below the horizon 
;
;
; KEYWORD INPUT:
;
;   local       if set, TIME is specified as a local time and SUNRISE
;               and SUNSET are output in terms of local time
; 
;               NOTE: "local time" is defined as UT + local_offset
;                     where local_offset is fix((lon+sign(lon)*7.5)/15)
;                     with -180 &lt; lon &lt; 180
;
;                     Be aware, there are no fancy provisions for
;                     day-light savings time and other such nonsense.
;
; KEYWORD OUTPUT:
;
;   sunrise     Time of sunrise (hours)
;   sunset      Time of sunset  (hours)
;
;   latsun      the latitude of the sub-solar point (fairly constant over day)
;               Note that daily_minimum_zenith=abs(latsun-lat)
;
;   lonsun      the longitude of the sub-solar point
;               Note that at 12 noon local time (lon-lonsun)/15. is the
;               number of minutes by which the sun leads the clock.
;
;;              Often used           lat,   lon 
;
;               Santa Barbara:        34.410,-119.727
;               SGP Cart Site:        36.605,-97.485
;               North Slope:          69.318,-151.015
;               Palmer Station:       -64.767,-64.067
; 
;; EXAMPLE 1:   Compute the solar flux at Palmer Station for day 283
;
;               time=findgen(1+24*60)/60
;               zensun,283,time,-64.767,-64.067,z,a,sf
;               solflx=sf*s
;               plot,time,solflx
;
;               where s is the TOA solar irradiance at normal incidence:
;
;               s=1618.8   ; W/m2/micron for AVHRR1 GTR100 
;               s= 976.9   ; W/m2/micron for AVHRR2 GTR100
;               s=1685.1   ; W/m2/micron for 410nm GTR100
;               s= 826.0   ; W/m2/micron for 936nm GTR100
;               s=1.257e17 ; photons/cm2/s PAR GTR100
;               s=1372.9   ; w/m2 total broadband
;
;
;; EXAMPLE 2:   Find time of sunrise and sunset for current day
; 
;     doy=julday()-julday(1,0,1994)                          
;     zensun,doy,12,34.456,-119.813,z,a,s,sunrise=sr,sunset=ss,/local &amp;$
;     zensun,doy,[sr,.5*(sr+ss),ss],34.456,-119.813,z,az,/local &amp;$
;     print,'sunrise: '+hms(3600*sr)+      ' PST   azimuth angle: ',az(0)  &amp;$
;     print,'sunset:  '+hms(3600*ss)+      ' PST   azimuth angle: ',az(2)  &amp;$
;     print,'zenith:  '+hms(1800*(ss+sr))+ ' PST   zenith angle:  ',z(1) 
;   
;  AUTHOR:      Paul Ricchiazzi        23oct92
;               Earth Space Research Group,  UCSB
; 
;  REVISIONS:
;  
; jan94: use spline fit to interpolate on EQT and DEC tables
; jan94: output SUNRISE and SUNSET, allow input/output in terms of local time 
; jan97: declare eqtime and decang as floats.  previous version 
;         this caused small offsets in the time of minimum solar zenith
;-
; PROCEDURE: 
;
; 1.  Calculate the subsolar point latitude and longitude, based on
;     DAY and TIME. Since each year is 365.25 days long the exact
;     value of the declination angle changes from year to year.  For
;     precise values consult THE AMERICAN EPHEMERIS AND NAUTICAL
;     ALMANAC published yearly by the U.S. govt. printing office.  The
;     subsolar coordinates used in this code were provided by a
;     program written by Jeff Dozier.
;
;  2. Given the subsolar latitude and longitude, spherical geometry is
;     used to find the solar zenith, azimuth and flux multiplier.
;
;  eqt = equation of time (minutes)  ; solar longitude correction = -15*eqt
;  dec = declination angle (degrees) = solar latitude 
;
; LOWTRAN v7 data (25 points)
;     The LOWTRAN solar position data is characterized by only 25 points.
;     This should predict the subsolar angles within one degree.  For
;     increased accuracy add more data points.
;
;nday=[   1.,    9.,   21.,   32.,   44.,   60.,  91.,  121.,  141.,  152.,$
;       160.,  172.,  182.,  190.,  202.,  213., 244.,  274.,  305.,  309.,$
;       325.,  335.,  343.,  355.,  366.]
;
;eqt=[ -3.23, -6.83,-11.17,-13.57,-14.33,-12.63, -4.2,  2.83,  3.57,  2.45,$
;       1.10, -1.42, -3.52, -4.93, -6.25, -6.28,-0.25, 10.02, 16.35, 16.38,$
;       14.3, 11.27,  8.02,  2.32, -3.23]
;
;dec=[-23.07,-22.22,-20.08,-17.32,-13.62, -7.88, 4.23, 14.83, 20.03, 21.95,$
;      22.87, 23.45, 23.17, 22.47, 20.63, 18.23, 8.58, -2.88,-14.18,-15.45,$
;     -19.75,-21.68,-22.75,-23.43,-23.07]
;
; Analemma information from Jeff Dozier
;     This data is characterized by 74 points
;

today = DATE_CONV(date , 'V')

day = today(1)


if n_params() eq 0 then begin
  xhelp,'zensun'
  return
endif  

nday=[  1.0,   6.0,  11.0,  16.0,  21.0,  26.0,  31.0,  36.0,  41.0,  46.0,$
       51.0,  56.0,  61.0,  66.0,  71.0,  76.0,  81.0,  86.0,  91.0,  96.0,$
      101.0, 106.0, 111.0, 116.0, 121.0, 126.0, 131.0, 136.0, 141.0, 146.0,$
      151.0, 156.0, 161.0, 166.0, 171.0, 176.0, 181.0, 186.0, 191.0, 196.0,$
      201.0, 206.0, 211.0, 216.0, 221.0, 226.0, 231.0, 236.0, 241.0, 246.0,$
      251.0, 256.0, 261.0, 266.0, 271.0, 276.0, 281.0, 286.0, 291.0, 296.0,$
      301.0, 306.0, 311.0, 316.0, 321.0, 326.0, 331.0, 336.0, 341.0, 346.0,$
      351.0, 356.0, 361.0, 366.0]

eqt=[ -3.23, -5.49, -7.60, -9.48,-11.09,-12.39,-13.34,-13.95,-14.23,-14.19,$
     -13.85,-13.22,-12.35,-11.26,-10.01, -8.64, -7.18, -5.67, -4.16, -2.69,$
      -1.29, -0.02,  1.10,  2.05,  2.80,  3.33,  3.63,  3.68,  3.49,  3.09,$
       2.48,  1.71,  0.79, -0.24, -1.33, -2.41, -3.45, -4.39, -5.20, -5.84,$
      -6.28, -6.49, -6.44, -6.15, -5.60, -4.82, -3.81, -2.60, -1.19,  0.36,$
       2.03,  3.76,  5.54,  7.31,  9.04, 10.69, 12.20, 13.53, 14.65, 15.52,$
      16.12, 16.41, 16.36, 15.95, 15.19, 14.09, 12.67, 10.93,  8.93,  6.70,$
       4.32,  1.86, -0.62, -3.23]

dec=[-23.06,-22.57,-21.91,-21.06,-20.05,-18.88,-17.57,-16.13,-14.57,-12.91,$
     -11.16, -9.34, -7.46, -5.54, -3.59, -1.62,  0.36,  2.33,  4.28,  6.19,$
       8.06,  9.88, 11.62, 13.29, 14.87, 16.34, 17.70, 18.94, 20.04, 21.00,$
      21.81, 22.47, 22.95, 23.28, 23.43, 23.40, 23.21, 22.85, 22.32, 21.63,$
      20.79, 19.80, 18.67, 17.42, 16.05, 14.57, 13.00, 11.33,  9.60,  7.80,$
       5.95,  4.06,  2.13,  0.19, -1.75, -3.69, -5.62, -7.51, -9.36,-11.16,$
     -12.88,-14.53,-16.07,-17.50,-18.81,-19.98,-20.99,-21.85,-22.52,-23.02,$
     -23.33,-23.44,-23.35,-23.06]

;
; compute the subsolar coordinates
;

tt=((fix(day)+time/24.-1.) mod 365.25) +1.  ;; fractional day number
                                            ;; with 12am 1jan = 1.

if n_elements(tt) gt 1 then begin
  eqtime=tt-tt                              ;; this used to be day-day, caused 
  decang=eqtime                             ;; error in eqtime &amp; decang when a
  ii=sort(tt)                               ;; single integer day was input
  eqtime(ii)=spline(nday,eqt,tt(ii))/60.    
  decang(ii)=spline(nday,dec,tt(ii))
endif else begin
  eqtime=spline(nday,eqt,tt)/60.
  decang=spline(nday,dec,tt)
endelse  
latsun=decang

if keyword_set(local) then begin
  lonorm=((lon + 360 + 180 ) mod 360 ) - 180.
  tzone=fix((lonorm+7.5)/15)
  index = where(lonorm lt 0, cnt)
  if (cnt gt 0) then tzone(index) = fix((lonorm(index)-7.5)/15)
  ut=(time-tzone+24.) mod 24.                  ; universal time
  noon=tzone+12.-lonorm/15.                    ; local time of noon
endif else begin
  ut=time
  noon=12.-lon/15.                             ; universal time of noon
endelse

lonsun=-15.*(ut-12.+eqtime)

; compute the solar zenith, azimuth and flux multiplier

t0=(90.-lat)*!dtor                            ; colatitude of point
t1=(90.-latsun)*!dtor                         ; colatitude of sun

p0=lon*!dtor                                  ; longitude of point
p1=lonsun*!dtor                               ; longitude of sun

zz=cos(t0)*cos(t1)+sin(t0)*sin(t1)*cos(p1-p0) ; up          \
xx=sin(t1)*sin(p1-p0)                         ; east-west    &gt; rotated coor
yy=sin(t0)*cos(t1)-cos(t0)*sin(t1)*cos(p1-p0) ; north-south /

azimuth=atan(xx,yy)/!dtor                     ; solar azimuth 
zenith=acos(zz)/!dtor                         ; solar zenith

rsun=1.-0.01673*cos(.9856*(tt-2.)*!dtor)      ; earth-sun distance in AU
solfac=zz/rsun^2                              ; flux multiplier

if n_elements(time) eq 1 then begin
  angsun=6.96e10/(1.5e13*rsun)               ; solar disk half-angle
  ;angsun=0.  
  arg=-(sin(angsun)+cos(t0)*cos(t1))/(sin(t0)*sin(t1))
  sunrise = arg - arg 
  sunset  = arg - arg + 24.
  index = where(abs(arg) le 1, cnt)
  if (cnt gt 0) then begin
    dtime=acos(arg(index))/(!dtor*15)
    sunrise(index)=noon-dtime-eqtime(index)
    sunset(index)=noon+dtime-eqtime(index)
  endif
endif
return
end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  
pro display, vars

  nVars = n_elements(vars)

  if (nVars eq 0) then return

  nchop = floor(alog10(nVars))+1

  for iVar = 0,nVars-1 do $
    print, chopr('0000'+tostr(iVar),nchop),'. ',vars(iVar)

end
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PRO c_a_to_r, timearray, timereal

  dayofmon = [31,28,31,30,31,30,31,31,30,31,30,31]
  if ((timearray(0) mod 4) eq 0) then dayofmon(1) = dayofmon(1) + 1

  timereal = double(0.0)

  if timearray(0) lt 65 then timearray(0) = timearray(0) + 2000
  if timearray(0) gt 1900 then numofyears = timearray(0)-1965 		      $
  else numofyears = timearray(0)-65	
  numofleap = floor(float(numofyears)/4.0)
  numofmonths = timearray(1) - 1
  numofdays = 0

  for i = 0, numofmonths-1 do begin

    numofdays = numofdays + dayofmon(i)

  endfor

  numofdays = numofdays + timearray(2) - 1
  numofhours = timearray(3)
  numofminutes = timearray(4)
  numofseconds = timearray(5)

  timereal = double(numofseconds*1.0) +       $
	     double(numofminutes*60.0) +             $
	     double(numofhours*60.0*60.0) +          $
	     double(numofdays*24.0*60.0*60.0) +      $
	     double(numofleap*24.0*60.0*60.0) +      $
	     double(numofyears*365.0*24.0*60.0*60.0)

  RETURN

END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
pro c_a_to_s, timearray, strtime

  mon='JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC' 

  sd = '0'+tostr(timearray(2))
  sd = strmid(sd,strlen(sd)-2,2)
  sm = strmid(mon,(timearray(1)-1)*3,3)
  if timearray(0) lt 1900 then year = timearray(0) 		$
  else year = timearray(0)-1900
  if (year ge 100) then year = year - 100
  sy = chopr('0'+tostr(year),2)
  sh = '0'+tostr(timearray(3))
  sh = strmid(sh,strlen(sh)-2,2)
  si = '0'+tostr(timearray(4))
  si = strmid(si,strlen(si)-2,2)
  ss = '0'+tostr(timearray(5))
  ss = strmid(ss,strlen(ss)-2,2)

  strtime = sd+'-'+sm+'-'+sy+' '+sh+':'+si+':'+ss+'.000'

  RETURN

END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
PRO c_r_to_a, timearray, timereal

  dayofmon = [31,28,31,30,31,30,31,31,30,31,30,31]

  timearray = intarr(6)

  speryear = double(31536000.0)
  sperday  = double(86400.0)
  sperhour = double(3600.0)
  spermin  = double(60.0)

  numofyears = floor(timereal/speryear)
  if (numofyears+65) mod 4 eq 0 then dayofmon(1) = dayofmon(1) + 1
  numofdays = floor((timereal mod speryear)/sperday)
  numofleap = floor(numofyears / 4)
  numofdays = numofdays - numofleap
  if numofdays lt 0 then begin
    if (numofyears+65) mod 4 eq 0 then dayofmon(1) = dayofmon(1) - 1
    numofyears = numofyears - 1
    numofdays = numofdays + numofleap + 365
    if (numofyears+65) mod 4 eq 0 then dayofmon(1) = dayofmon(1) + 1
    numofleap = floor(numofyears / 4)
    numofdays = numofdays - numofleap
  endif
  numofhours = floor((timereal mod sperday)/sperhour)
  numofmin = floor((timereal mod sperhour)/spermin)
  numofsec = floor(timereal mod spermin)

  numofmon = 0

  while numofdays ge dayofmon(numofmon) do begin

    numofdays = numofdays - dayofmon(numofmon)
    numofmon = numofmon + 1

  endwhile

  timearray(0) = numofyears + 1965
  timearray(1) = numofmon + 1
  timearray(2) = numofdays + 1
  timearray(3) = numofhours
  timearray(4) = numofmin
  timearray(5) = numofsec

  RETURN

END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
pro c_s_to_a, timearray, strtime

  timearray = intarr(6)

  mon='JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC'

  timearray(2) = fix(strmid(strtime,0,2))
  smon = strmid(strtime,3,3)
  bs = byte(smon)
  loc = where((bs ge 97) and (bs lt 122), count)
  if count gt 0 then bs(loc) = bs(loc)-byte(32)
  smon = string(bs)

  for j=0,11 do 							      $
    if strmid(mon,j*3,3) eq smon then timearray(1)=j+1
  timearray(0)=fix(strmid(strtime,7,2))+1900
  if timearray(0) lt 1965 then timearray(0) = timearray(0) + 100

  if (strlen(strmid(strtime,10,2)) gt 0) and			$
     (strmid(strtime,10,2) ne '  ') then			$
    timearray(3)=fix(strmid(strtime,10,2))			$
  else timearray(3) = 0
  if (strlen(strmid(strtime,13,2)) gt 0) and			$
     (strmid(strtime,13,2) ne '  ') then			$
    timearray(4)=fix(strmid(strtime,13,2))			$
  else timearray(4) = 0
  if (strlen(strmid(strtime,16,6)) gt 0) and			$
     (strmid(strtime,16,2) ne '  ') then			$
    timearray(5)=fix(strmid(strtime,16,6))			$
  else timearray(5) = 0

  RETURN

END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
pro pos_space, nb, space, sizes, nx = nx, ny = ny

; pos_space
;
; Determines the size and multiplication factors for plotting perfect circles
; or squares. This routine is used to simply find the parameters, another
; procedure, get_position, is used to actually find the position of the 
; circle or square.
; This routine maxamizes the area used by the plots, determinining the best
; positions for the number of plots that the user has selected.
;
; input parameters:
; nb - number of plots on a page
; space - amount of space in between each of the plots in normalized
;	  coordinates
;
; output parameters:
; bs - box size (size of the plotting region)
; nbx, nby - number of plots in the x and y directions
; xoff, yoff - x and y offsets for positions
; xf, yf - x and y multiplication factors for making perfect squares
;
; This has been adapted to allow the user to define how many objects
;   are in the x and y direction on Jan 2, 1998
  
  sizes = {bs:0.0, nbx:0, nby:0, xoff:0.0, yoff:0.0, xf:0.0, yf:0.0}

  xsi = float(!d.x_size)
  ysi = float(!d.y_size)

  xs = xsi - 5.0*space*xsi
  ys = ysi - 5.0*space*ysi

  if nb eq 1 then begin

    sizes.nbx = 1
    sizes.nby = 1
    sizes.bs = 1.0 - space

    if xs gt ys then begin

       sizes.yf = 1.0
       sizes.xf = ys/xs

    endif else begin

       sizes.xf = 1.0
       sizes.yf = xs/ys

     endelse

  endif else begin

    if (n_elements(nx) gt 0) then begin
      sizes.nbx = nx(0)
      if n_elements(ny) eq 0 then sizes.nby = nb/nx(0) else sizes.nby = ny(0)
    endif else begin
      if (n_elements(ny) gt 0) then begin
        sizes.nby = ny(0)
        sizes.nbx = nb/ny(0)
      endif else begin
        if xs gt ys then begin
          sizes.nbx = round(sqrt(nb))
          sizes.nby = fix(nb/sizes.nbx)
        endif else begin
          sizes.nby = round(sqrt(nb))
          sizes.nbx = fix(nb/sizes.nby)
        endelse
      endelse
    endelse

    if xs gt ys then begin

      if (sizes.nbx*sizes.nby lt nb) then 				$
	if (sizes.nbx le sizes.nby) then sizes.nbx = sizes.nbx + 1	$
	else sizes.nby = sizes.nby + 1					$
      else								$
	if (sizes.nbx lt sizes.nby) and					$
	   (n_elements(nx) eq 0) and					$
	   (n_elements(ny) eq 0) then begin
	  temp = sizes.nby
	  sizes.nby = sizes.nbx
	  sizes.nbx = temp
	endif

      sizes.yf = 1.0
      sizes.xf = ys/xs
      sizes.bs = ((1.0-space*(sizes.nbx-1))/sizes.nbx )/sizes.xf
      if sizes.nby*sizes.bs+space*(sizes.nby-1) gt 1.0 then 		$
	sizes.bs = (1.0- space*(sizes.nby-1))/sizes.nby 

    endif else begin

      if (sizes.nbx*sizes.nby lt nb) then				$
	if (sizes.nby le sizes.nbx) then sizes.nby = sizes.nby + 1	$
	else sizes.nbx = sizes.nbx + 1					$
      else								$
	if (sizes.nby lt sizes.nbx) and					$
	   (n_elements(nx) eq 0) and					$
	   (n_elements(ny) eq 0) then begin
	  temp = sizes.nby
	  sizes.nby = sizes.nbx
	  sizes.nbx = temp
	endif

      sizes.xf = 1.0
      sizes.yf = xs/ys
      sizes.bs = ((1.0 - space*(sizes.nby-1))/sizes.nby)/sizes.yf
      if sizes.nbx*sizes.bs+space*(sizes.nbx-1) gt 1.0 then 		$
	sizes.bs = (1.0 - space*(sizes.nbx-1))/sizes.nbx

    endelse

  endelse

  sizes.xoff = (1.0 - sizes.xf*(sizes.bs*sizes.nbx + space*(sizes.nbx-1)))/2.0
  sizes.yoff = (1.0 - sizes.yf*(sizes.bs*sizes.nby + space*(sizes.nby-1)))/2.0

  RETURN

END
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
