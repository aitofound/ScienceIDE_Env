!#######################################################################
!
!  EXPMAC:  Fortran 90 pre-processing macro expander.
!
!#######################################################################
! Copyright 2025 Predictive Science Inc.
!
! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at
!
!    http://www.apache.org/licenses/LICENSE-2.0
!
! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
! implied.
! See the License for the specific language governing permissions and
! limitations under the License.
!#######################################################################
!
!-----------------------------------------------------------------------
!
! ****** Changelog:
!
!        02/10/2000, ZM, Version 1.00:
!
!         - Original version of program.
!           Added a command-line interface to the original version.
!
!        07/29/2003, ZM, Version 1.01:
!
!         - This version uses the ZM tools libraries.
!
!        04/25/2012, ZM, Version 1.02:
!
!         - Added the option of specifying macro definitions in a
!           separate file from the source file.  This is useful when
!           a code is split into individual source files for each
!           routine.
!         - Improved the formatting of the debugging information.
!         - Performed a slight cleanup of the code.
!
!        08/16/2012, ZM, Version 1.03:
!
!         - Fixed some minor bugs that were discovered by compiling
!           with GFORTRAN.
!
!        04/28/2021, RC, Version 1.03r:
!
!         - Manually added parse and number type module code
!           so this tool is a stand-alone release.
!
!        04/02/2025, RC, Version 2.0.0:
!
!         - Updated code to work for Fortran 90 continuation and
!           comments syntax.
!           This is for GIT version of MAS.  It will not work for the
!           old versions of MAS.
!
!        04/03/2025, RC, Version 2.1.0:
!
!         - Removed processing of non-macro lines since normal line
!           lengths are not a problem now that MAS is Fortran 90.
!           Macros can exceed F90 lengths so CPP still not a good
!           option, and this code is still needed.
!
!#######################################################################
module number_types
!
      use iso_fortran_env
!
      implicit none
!
!
! ****** Set up the KIND values for the various REALs.
! ****** These can be compiler dependent.
!
      integer, parameter :: KIND_REAL_4=REAL32
      integer, parameter :: KIND_REAL_8=REAL64
!
! ****** KIND values for specifying the precision of REALs.
!
      integer, private, parameter :: r4=KIND_REAL_4
      integer, private, parameter :: r8=KIND_REAL_8
!
! ****** Select the number type for REALs (one of: R4|R8).
!
      integer, parameter :: r_typ=r8
!
end module
!#######################################################################
module syntax
!
!-----------------------------------------------------------------------
! ****** Group definitions for parsing command-line arguments.
!-----------------------------------------------------------------------
!
!        GROUP 1: <kw>
!        GROUP 2: <arg>
!        GROUP 3: <kw> <arg>
!        GROUP 4: <kw> <arg> <arg>
!
      integer, parameter :: ngroups=4
!
      integer, parameter :: GROUP_K  =1
      integer, parameter :: GROUP_A  =2
      integer, parameter :: GROUP_KA =3
      integer, parameter :: GROUP_KAA=4
!
end module
!#######################################################################
module string_def
!
!-----------------------------------------------------------------------
! ****** Define a structure to hold a string.
!-----------------------------------------------------------------------
!
      implicit none
!
      type :: string
        character, dimension(:), pointer :: c
      end type
!
end module
!#######################################################################
module paragraph_def
!
      use string_def
!
      implicit none
!
!-----------------------------------------------------------------------
! ****** Define a structure for a linked list of lines
! ****** (i.e., a paragraph).
!-----------------------------------------------------------------------
!
      type :: paragraph
        type(string) :: line
        type(paragraph), pointer :: next
      end type
!
!-----------------------------------------------------------------------
! ****** Define a structure to hold a list of paragraphs.
!-----------------------------------------------------------------------
!
      type :: parlist
        type(paragraph), pointer :: par
      end type
!
end module
!#######################################################################
module lcase_interface
      interface
        function lcase (s)
        character(*), intent(in) :: s
        character(len(s)) :: lcase
        end
      end interface
end module
!#######################################################################
module ucase_interface
      interface
        function ucase (s)
        character(*), intent(in) :: s
        character(len(s)) :: ucase
        end
      end interface
end module
!#######################################################################
module new_par_interface
      interface
        subroutine new_par (par)
        use paragraph_def
        implicit none
        type(paragraph), pointer :: par
        end
      end interface
end module
!#######################################################################
module delete_par_interface
      interface
        subroutine delete_par (par)
        use paragraph_def
        implicit none
        type(paragraph), pointer :: par
        end
      end interface
end module
!#######################################################################
module add_line_interface
      interface
        subroutine add_line (line,par)
        use paragraph_def
        implicit none
        character(*) :: line
        type(paragraph), pointer :: par
        end
      end interface
end module
!#######################################################################
module print_par_interface
      interface
        subroutine print_par (par)
        use paragraph_def
        implicit none
        type(paragraph), pointer :: par
        end
      end interface
end module
!#######################################################################
module get_str_interface
      interface
        function get_str (str)
        use string_def
        implicit none
        type(string) :: str
        character(size(str%c)) :: get_str
        end
      end interface
end module
!#######################################################################
module get_usage_line_interface
      interface
        subroutine get_usage_line (usage)
        use paragraph_def
        implicit none
        type(paragraph), pointer :: usage
        end
      end interface
end module
!#######################################################################
module params
!
!-----------------------------------------------------------------------
! ****** Parameters.
!-----------------------------------------------------------------------
!
      implicit none
!
! ****** File names.
!
      character(512) :: infile
      character(512) :: outfile
      character(512) :: macro_def_file
!
! ****** Debug level.
!
      integer :: idebug
!
end module
!#######################################################################
module get_statement_vars
!
      implicit none
!
!-----------------------------------------------------------------------
! ****** Define the maximum number of characters allowed in a line.
!-----------------------------------------------------------------------
!
      integer, parameter :: LMX=132
!
! ****** Input line buffer.
!
      character*(LMX) :: line
!
!-----------------------------------------------------------------------
! ****** GETSTMT state variables.
!-----------------------------------------------------------------------
!
! ****** Current line number.
!
      integer :: ln
!
! ****** Switch to continue reading a statement.
!
      logical :: rdfil
!
!-----------------------------------------------------------------------
!
end module
!#######################################################################
module macro_defs
!
!-----------------------------------------------------------------------
! ****** Macro definition tables.
!-----------------------------------------------------------------------
!
      implicit none
!
      integer, parameter :: n_macros_max=100
      integer, parameter :: LMNAM=64
      integer, parameter :: LMRPL=512
      integer, parameter :: LMARG=64
!
      integer :: nmac=0
      character(LMNAM), dimension(n_macros_max) :: macnam
      character(LMRPL), dimension(n_macros_max) :: macrpl
      integer, dimension(n_macros_max) :: macnarg
!
end module
!#######################################################################
module buffers
!
!-----------------------------------------------------------------------
! ****** Input and output line buffers.
!-----------------------------------------------------------------------
!
      implicit none
!
      integer, parameter :: LBUF=20480
      character(LBUF) :: ibuf,obuf
!
end module
!#######################################################################
program EXPMAC
!
!-----------------------------------------------------------------------
!
      use params
      use macro_defs
      use buffers
!
!-----------------------------------------------------------------------
!
      character(*), parameter :: cname='EXPMAC'
      character(*), parameter :: cvers='2.1.0'
      character(*), parameter :: cdate='04/03/2025'
!
!-----------------------------------------------------------------------
!
      logical :: mdef,mres
      integer :: ierr
      character(512) :: line
!
!-----------------------------------------------------------------------
!
      logical, external :: macdef,macres
!
!-----------------------------------------------------------------------
!
! ****** Set the parameters.
!
      call set_parameters (cname,cvers,cdate)
!
! ****** Check that the input and output file names are not identical.
!
      if (infile.eq.outfile) then
        write (*,*)
        write (*,*) '### ERROR in ',cname,':'
        write (*,*) '### Input and output file names are identical.'
        stop (1)
      end if
!
! ****** If a separate macro definition file was specified,
! ****** parse the macros contained in it.
!
      if (macro_def_file.ne.' ') then
        call read_macro_file (macro_def_file)
      end if
!
! ****** Open the input and output files.
!
      call ffopen (1,infile,'r',ierr)
!
      if (ierr.ne.0) then
        write (*,*)
        write (*,*) '### ERROR in ',cname,':'
        write (*,*) '### Could not open the input file.'
        write (*,*) 'File name: ',trim(infile)
        stop (1)
      end if
!
      call ffopen (2,outfile,'rw',ierr)
!
      if (ierr.ne.0) then
        write (*,*)
        write (*,*) '### ERROR in ',cname,':'
        write (*,*) '### Could not open the output file.'
        write (*,*) 'File name: ',trim(outfile)
        stop (1)
      end if
!
! ****** Initialization.
!
      ls=0
      call getstmt (0,0,ibuf,ixi,ixfc,ierr)
!
! ****** Main loop.
!
  100 continue
!
! ****** Get the next statement.
!
      call getstmt (1,2,ibuf,ixi,ixfc,ierr)
      if (ierr.ne.0) go to 900
!
      ls=ls+1
!
      if (idebug.ge.3) then
        write (*,*)
        write (*,*) '### Input file: read a statement:'
        write (*,*) 'Statement #',ls,':'
        write (*,*) ibuf(1:ixi-1)
      end if
!
! ****** Check the line for macro calls.
!
      ixo=1
      mres=macres(ibuf(1:ixi-1),obuf,ixo)
      if (mres) then
        if (idebug.ge.2) then
          write (*,*)
          write (*,*) '### Input file: macro call:'
          write (*,*) 'Statement #',ls,':'
          write (*,*) 'Replacement text:'
          write (*,*) obuf(1:ixo-1)
        end if
      end if
!
! ****** Check if it is a macro definition.
!
      mdef=macdef(obuf(1:ixo-1))
      if (mdef) then
!
        if (idebug.ge.1) then
          write (*,*)
          write (*,*) '### Input file: macro definition:'
          write (*,*) 'Statement #',ls,':'
          write (*,*) 'Macro number = ',nmac
          write (*,*) 'Macro name = ',macnam(nmac)
          write (*,*) 'Number of arguments = ',macnarg(nmac)
          write (*,*) 'Replacement text:'
          lrpl=lenstr(macrpl(nmac))
          write (*,*) macrpl(nmac)(1:lrpl)
        end if
!
      else
        if (mres) then
!
! ****** Write the line to the output file.
!
          line=' '
          ix=ixfc-1
          do i=1,ixo-1
            ix=ix+1
            line(ix:ix)=obuf(i:i)
            if (ix.gt.72.and.(ixo-1+ixfc-1).gt.74) then
              write (2,'(a)') line(1:ix)//'&'
              ix=1
              line=' '
              line(1:1)='&'
            end if
          enddo
          if (ix.gt.0) then
            if (line(1:ix).ne.'&') write (2,'(a)') line(1:ix)
          end if

        else
!
! ******  Since this is now for F90, no need to enforce 73 column length
! ******  for non-macro code lines.
!
          line=' '
          ix=ixfc-1
          do i=1,ixo-1
            ix=ix+1
            line(ix:ix)=obuf(i:i)
          enddo
!
          write (2,'(a)') line(1:ix)

        end if
!
      end if
!
      go to 100
!
  900 continue
!
      close (1)
      close (2)
!
end program
!#######################################################################
subroutine read_macro_file (fname)
!
!-----------------------------------------------------------------------
!
! ****** Read macro definitions from file FNAME.
!
!-----------------------------------------------------------------------
!
      use params
      use get_statement_vars
      use macro_defs
      use buffers
!
!-----------------------------------------------------------------------
!
      character(*) :: fname
!
!-----------------------------------------------------------------------
!
      integer :: ierr
      integer :: ixi,ixfc,ixo,ls
      logical :: mres,mdef
!
!-----------------------------------------------------------------------
!
      logical, external :: macdef,macres
!
!-----------------------------------------------------------------------
!
! ****** Open the macro definition file.
!
      call ffopen (3,fname,'r',ierr)
!
      if (ierr.ne.0) then
        write (*,*)
        write (*,*) '### ERROR in READ_MACRO_FILE:'
        write (*,*) '### Could not open the macro definition file:'
        write (*,*) 'File name: ',trim(fname)
        stop (1)
      end if
!
      if (idebug.ge.1) then
        write (*,*)
        write (*,*) '### Reading macro definition file: ',trim(fname)
      end if
!
! ****** Initialization.
!
      ls=0
      call getstmt (0,0,ibuf,ixi,ixfc,ierr)
!
! ****** Main loop.
!
  100 continue
!
! ****** Get the next statement.
!
      call getstmt (3,0,ibuf,ixi,ixfc,ierr)
      if (ierr.ne.0) go to 900
!
      ls=ls+1
!
      if (idebug.ge.3) then
        write (*,*)
        write (*,*) '### Macro definition file: read a statement:'
        write (*,*) 'Statement #',ls,':'
        write (*,*) ibuf(1:ixi-1)
      end if
!
! ****** Check the line for macro calls.
!
      ixo=1
      mres=macres(ibuf(1:ixi-1),obuf,ixo)
      if (mres) then
        if (idebug.ge.2) then
          write (*,*)
          write (*,*) '### Macro definition file: macro call:'
          write (*,*) 'Statement #',ls,':'
          write (*,*) 'Replacement text:'
          write (*,*) obuf(1:ixo-1)
        end if
      end if
!
! ****** Check if it is a macro definition.
!
      mdef=macdef(obuf(1:ixo-1))
      if (mdef) then
!
        if (idebug.ge.1) then
          write (*,*)
          write (*,*) '### Macro definition file: macro definition:'
          write (*,*) 'Statement #',ls,':'
          write (*,*) 'Macro number = ',nmac
          write (*,*) 'Macro name = ',macnam(nmac)
          write (*,*) 'Number of arguments = ',macnarg(nmac)
          write (*,*) 'Replacement text:'
          lrpl=lenstr(macrpl(nmac))
          write (*,*) macrpl(nmac)(1:lrpl)
        end if
!
      else
!
! ****** It is a fatal error to have statments in the macro
! ****** definition file that do not define a macro.
!
        write (*,*)
        write (*,*) '### ERROR in READ_MACRO_FILE:'
        write (*,*) '### Encountered a statement that does not'// &
                    ' define a macro:'
        write (*,*) obuf(1:ixo-1)
        stop (1)
!
      end if
!
      go to 100
!
  900 continue
!
      close (3)
!
end subroutine
!#######################################################################
      logical function getname (line,ix,name,isl,delim)
!
!-----------------------------------------------------------------------
!
      character*(*) line,name
      logical delim
!
!-----------------------------------------------------------------------
!
      logical letter,alpha
!
!-----------------------------------------------------------------------
!
      getname=.false.
      delim=.false.
      name=' '
!
      lline=lenstr(line)
      lname=len(name)
!
! ****** Find the next name.
!
  100 continue
      if (ix.gt.lline) return
      if (.not.letter(line(ix:ix))) then
        if (isl.gt.0.and.line(ix:ix).eq.']') then
          delim=.true.
          return
        end if
        ix=ix+1
        go to 100
      end if
!
! ****** Get the name.
!
      ln=1
      name(1:1)=line(ix:ix)
      ix=ix+1
      getname=.true.
  200 continue
      if (ix.gt.lline) return
      if (alpha(line(ix:ix))) then
        ln=ln+1
        if (ln.gt.lname) then
          write (*,*)
          write (*,*) '### ERROR in GETNAME:'
          write (*,*) '### A variable name exceeds the maximum'// &
                      ' number of characters allowed.'
          write (*,*) 'Number of characters allowed = ',lname
          write (*,*) 'Truncated variable name: ',name
          stop (1)
        end if
        name(ln:ln)=line(ix:ix)
        ix=ix+1
        go to 200
      end if
!
      return
      end
!#######################################################################
      logical function alpha (ch)
!
!-----------------------------------------------------------------------
!
      character ch
!
!-----------------------------------------------------------------------
!
      alpha=.false.
!
      ich=ichar(ch)
!
      if ((ich.ge.65.and.ich.le. 90).or. &
          (ich.ge.97.and.ich.le.122).or. &
          (ich.ge.48.and.ich.le. 57).or. &
          ich.eq.95) alpha=.true.
!
      return
      end
!#######################################################################
      logical function letter (ch)
!
!-----------------------------------------------------------------------
!
      character ch
!
!-----------------------------------------------------------------------
!
      letter=.false.
!
      ich=ichar(ch)
!
      if ((ich.ge.65.and.ich.le. 90).or. &
          (ich.ge.97.and.ich.le.122)) letter=.true.
!
      return
      end
!#######################################################################
function lenstr (str)
!
!-----------------------------------------------------------------------
!
! ****** Get the length of string STR, ignoring right-filling blanks.
!
!-----------------------------------------------------------------------
!
      character*(*) str
!
!-----------------------------------------------------------------------
!
      do i=len(str),1,-1
        if (str(i:i).ne.' ') go to 100
      enddo
      i=0
  100 continue
!
      lenstr=i
!
      return
end function
!#######################################################################
function match (table,n,alpha)
!
      character*(*) table(n),alpha
!
      do i=1,n
        if (table(i).eq.alpha) then
          match=i
          return
        end if
      enddo
!
      match=0
!
      return
end function
!#######################################################################
      logical function find (line,ch,ix)
!
!-----------------------------------------------------------------------
!
! ****** Find the index of the first occurence of
! ****** character CH in LINE, starting from position IX.
!
! ****** If found, set FIND=.true., and set IX to be its index.
!
!-----------------------------------------------------------------------
!
      character*(*) line
      character ch
!
!-----------------------------------------------------------------------
!
      find=.false.
!
      ll=lenstr(line)
!
      ix0=ix
!
      do i=ix0,ll
        if (line(i:i).eq.ch) then
          ix=i
          find=.true.
          return
        end if
      enddo
!
      return
end function
!#######################################################################
function ifind (line,ch1,ch2,ix)
!
!-----------------------------------------------------------------------
!
! ****** Find the index of the first occurence of
! ****** character CH1 or CH2 in LINE, starting from
! ****** position IX.
!
! ****** Set IFIND=1 if CH1 was found, or set IFIND=2 if
! ****** CH2 was found, and set IX to be its index.
! ****** Set IFIND=0 if neither character was found.
!
!-----------------------------------------------------------------------
!
      character*(*) line
      character ch1,ch2
!
!-----------------------------------------------------------------------
!
      ifind=0
!
      ll=lenstr(line)
!
      ix0=ix
!
      do i=ix0,ll
        if (line(i:i).eq.ch1) then
          ix=i
          ifind=1
          return
        else if (line(i:i).eq.ch2) then
          ix=i
          ifind=2
          return
        end if
      enddo
!
      return
end function
!#######################################################################
subroutine addtobuf (line,buf,ix,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Add the string in LINE to the buffer BUF, starting
! ****** at character position IX in BUF.
!
!-----------------------------------------------------------------------
!
! ****** When COMPRESS=.true., multiple spaces in LINE are
! ****** converted into a single space.
!
!-----------------------------------------------------------------------
!
      logical COMPRESS
      parameter (COMPRESS=.false.)
!
!-----------------------------------------------------------------------
!
      character*(*) line,buf
!
!-----------------------------------------------------------------------
!
      logical psp
!
!-----------------------------------------------------------------------
!
      ierr=0
!
      lbuf=len(buf)
      ll=len(line)
!
      psp=.false.
      do i=1,ll
!
        if (COMPRESS) then
!
! ****** Compress multiple spaces into a single space.
!
          if (line(i:i).eq.' ') then
            if (psp) continue
            psp=.true.
          else
            psp=.false.
          end if
        end if
!
! ****** Check for a buffer overflow.
!
        if (ix.gt.lbuf) then
          ierr=1
          return
        end if
!
! ****** Write the character to the buffer.
!
        buf(ix:ix)=line(i:i)
        ix=ix+1
!
      enddo
!
end subroutine
!#######################################################################
      integer function fchar (str)
!
!-----------------------------------------------------------------------
!
! ****** Find the position of the first non-blank character in STR.
!
!-----------------------------------------------------------------------
!
      character*(*) str
!
!-----------------------------------------------------------------------
!
      do i=1,len(str)
        if (str(i:i).ne.' ') go to 100
      enddo
      i=0
  100 continue
!
      fchar=i
!
      return
      end
!#######################################################################
subroutine getstmt (iun,iuno,buf,ix,ixfc,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Return the next whole FORTRAN statement from the
! ****** file connected to unit IUN.
!
!-----------------------------------------------------------------------
!
! ****** The statement is returned in BUF.  IX-1 is the index
! ****** of the last character filled in BUF.
!
! ****** If a statement is returned, IERR=0 is set.
! ****** IERR=1 is returned when no more statements can be
! ****** found in the file.
!
! ****** IXFC is the position of the first non-blank character
! ****** in the statement.  BUF is filled with the statement
! ****** without including any leading spaces.
! ****** Multiple spaces are combined into a single space.
!
! ****** This routine skips blank lines.
! ****** If a statement is a comment, it is written
! ****** (without any processing) to unit IUNO if IUNO>0.
! ****** Set IUNO=0 to exclude comments from the output file.
!
! ****** This routine must be initialized with IUN=0
! ****** before it is used to return the first statement.
!
!-----------------------------------------------------------------------
!
      use get_statement_vars
!
!-----------------------------------------------------------------------
!
      character(*) :: buf
!
!-----------------------------------------------------------------------
!
! ****** Backslash character ("\").
!
      character(1), parameter :: bslash=achar(92)
!
!-----------------------------------------------------------------------
!
      logical :: defcont
!
!-----------------------------------------------------------------------
!
      integer, external :: fchar
      logical, external :: rdlin
!
!-----------------------------------------------------------------------
!
      ierr=0
!
! ****** Check if this is an initialization.
!
      if (iun.eq.0) then
        rdfil=.true.
        ln=0
        return
      end if
!
      lbuf=len(buf)
      defcont=.false.
      ix=1
!
  100 continue
!
! ****** Get the next line.
!
      if (rdfil) then
        if (.not.rdlin(iun,line)) then
          ierr=1
          return
        end if
        ln=ln+1
      end if
!
! ****** Discard blank lines.
!
      ll=lenstr(line)
      if (ll.eq.0) go to 100
!
! ****** Check if it is a continued "#define" line.
!
      if (defcont) then
        if (line(ll:ll).eq.bslash) then
          call addtobuf (line(1:ll-1),buf,ix,ierr)
          if (ierr.ne.0) go to 900
          go to 100
        else
          defcont=.false.
          call addtobuf (line(1:ll),buf,ix,ierr)
          if (ierr.ne.0) go to 900
          go to 300
        end if
      else
        if (ll.ge.8.and.line(1:7).eq.'#define'.and. &
            line(ll:ll).eq.bslash) then
          defcont=.true.
          call addtobuf (line(1:ll-1),buf,ix,ierr)
          if (ierr.ne.0) go to 900
          rdfil=.true.
          go to 100
        end if
      end if
!
! ****** If it is a comment, write it out to unit IUNO
! ****** and process the next line.
!
      if (line(1:1).eq.'c'.or.line(1:1).eq.'C'.or.line(1:1).eq.'!') then
        if (iuno.gt.0) write (iuno,'(a)') line(1:ll)
        rdfil=.true.
        go to 100
      end if
!
! ****** Find the position of the first non-blank character.
!
      ixfc=fchar(line(1:ll))
!
! ****** Check length of line.
!
!      if (ll.gt.72) then
!        write (*,*)
!        write (*,*) '### WARNING from GETSTMT:'
!        write (*,*) '### Line exceeds 72 characters.'
!        write (*,*) 'Number of characters in line = ',ll
!        write (*,*) 'Line number = ',ln
!        write (*,*) 'The line is:'
!        write (*,*) line(1:ll)
!      end if
!
! ****** Add the line to the buffer.
!
      call addtobuf (line(ixfc:ll),buf,ix,ierr)
      if (ierr.ne.0) go to 900
!
! ****** Read the next line to check if it is a continuation.
!
  200 continue
!
      if (.not.rdlin(iun,line)) then
        rdfil=.true.
        go to 300
      end if
!
      ln=ln+1
!
! ****** Discard blank lines.
!
      ll=lenstr(line)
      if (ll.eq.0) go to 200
!
! ****** Check if it is a comment.
!
      if (line(1:1).eq.'c'.or.line(1:1).eq.'C'.or.line(1:1).eq.'!') then
        rdfil=.false.
        return
      end if
!
! ****** Check if it is a continuation.
!
      if (ll.ge.6.and.line(1:5).eq.' '.and.line(6:6).ne.' ') then
        call addtobuf (line(7:ll),buf,ix,ierr)
        if (ierr.ne.0) go to 900
        go to 200
      else
        rdfil=.false.
      end if
!
  300 continue
!
      return
!
  900 continue
!
! ****** Error: output buffer overflow.
!
      write (*,*)
      write (*,*) '### ERROR in GETSTMT:'
      write (*,*) '### Line buffer overflow.'
      write (*,*) '[A statement may be too long.]'
      write (*,*) 'Line number = ',ln
      stop (1)
!
end subroutine
!#######################################################################
      logical function rdlin (iun,line)
!
!-----------------------------------------------------------------------
!
! ****** Read a line of text from unit IUN.
!
! ****** If a line was read successfully, return with
! ****** RDLIN=.true.; otherwise, set RDLIN=.false.
!
!-----------------------------------------------------------------------
!
      character*(*) line
!
!-----------------------------------------------------------------------
!
      read (iun,'(a)',end=100,err=100) line
!
      rdlin=.true.
      return
!
  100 continue
!
! ****** Error while reading the line.
!
      rdlin=.false.
!
      return
      end
!#######################################################################
      logical function macdef (buf)
!
!-----------------------------------------------------------------------
!
! ****** Check if the buffer BUF holds a macro definition.
!
!-----------------------------------------------------------------------
!
! ****** If this is a valid macro definition, add it to the
! ****** macro tables.
!
! ****** A valid macro has the following definition:
! ******
! ******    #define <name>(arg1[,arg2]*) <replacement text>
! ******
! ****** The line can be continued by a "\".  The "#define"
! ****** must start in column 1.  The macro can have 1 or more
! ****** arguments. A "##" in the replacement text concatenates
! ****** its neighbors.
!
!-----------------------------------------------------------------------
!
      use macro_defs
!
!-----------------------------------------------------------------------
!
      character*(*) buf
!
!-----------------------------------------------------------------------
!
      character*(LMNAM) name
!
      parameter (nargmx=20)
      character*(LMARG) args(nargmx),targ
!
!-----------------------------------------------------------------------
!
      external getname,fchar,find
      logical getname,find
      integer fchar
      logical :: delim
!
!-----------------------------------------------------------------------
!
      character(3) ch3
!
!-----------------------------------------------------------------------
!
      macdef=.false.
!
      narg=0
      lb=len(buf)
!
      if (.not.(lb.gt.8.and.buf(1:8).eq.'#define ')) return
      ix=9
      ix0=ix
      if (.not.getname(buf,ix,name,0,delim)) return
      ln=lenstr(name)
      if (ix-1-ln.ge.ix0.and.buf(ix0:ix-1-ln).ne.' ') return
      ix0=ix
      if (.not.find(buf,'(',ix)) return
      if (ix-1.ge.ix0.and.buf(ix0:ix-1).ne.' ') return
      ix=ix+1
!
      nmac=nmac+1
!
      if (nmac.gt.n_macros_max) then
        write (*,*)
        write (*,*) '### ERROR in MACDEF:'
        write (*,*) '### The maximum allowed number of macros'// &
                    ' has been exceeded:'
        write (*,*) '### Maximum number allowed = ',n_macros_max
        write (*,*)
        write (*,*) '### The input line is:'
        write (*,*) trim(buf)
        stop (1)
      end if
!
      macnam(nmac)=name
!
! ****** Collect the arguments.
!
  100 continue
!
      ix0=ix
      ifnd=ifind(buf,')',',',ix)
      if (ifnd.eq.0) then
        nmac=nmac-1
        return
      end if
      narg=narg+1
      if (narg.gt.nargmx) then
        write (*,*)
        write (*,*) '### ERROR in MACDEF:'
        write (*,*) '### Too many macro arguments.'
        write (*,*) 'Argument number = ',narg
        write (*,*) 'Input line is:'
        write (*,*) buf
        stop (1)
      end if
      larg=ix-ix0
      if (larg.gt.LMARG) then
        write (*,*)
        write (*,*) '### ERROR in MACDEF:'
        write (*,*) '### Macro argument is too long.'
        write (*,*) 'Argument number = ',narg
        write (*,*) 'Argument length = ',larg
        write (*,*) 'Input line is:'
        write (*,*) buf
        stop (1)
      end if
      args(narg)=buf(ix0:ix-1)
      args(narg)=adjustl(args(narg))
      ix=ix+1
      if (ifnd.eq.2) go to 100
!
      macnarg(nmac)=narg
!
! ****** Store the replacement text, flagging the arguments
! ****** with $<argnum>, where <argnum> is a 2-digit number.
!
! ****** Remove leading spaces.
!
      ic0=fchar(buf(ix:))
      ix=ix+ic0-1
!
      ixr=1
      macrpl(nmac)=' '
!
  200 continue
!
      ix0=ix
      if (getname(buf,ix,targ,0,delim)) then
        iarg=match(args,narg,targ)
        if (iarg.ne.0) then
!
! ****** Write the text and code the argument.
!
          la=lenstr(targ)
          call addtobuf (buf(ix0:ix-1-la),macrpl(nmac),ixr,ierr)
          if (ierr.ne.0) go to 900
          ch3(1:1)='$'
          write (ch3(2:3),'(i2.2)') iarg
          call addtobuf (ch3,macrpl(nmac),ixr,ierr)
          if (ierr.ne.0) go to 900
!
        else
!
! ****** Write the text.
!
          call addtobuf (buf(ix0:ix-1),macrpl(nmac),ixr,ierr)
          if (ierr.ne.0) go to 900
!
        end if
        go to 200
      else
        call addtobuf (buf(ix0:lb),macrpl(nmac),ixr,ierr)
        if (ierr.ne.0) go to 900
      end if
!
! ****** Resolve the concatenation operator "##".
!
      call filtnn (macrpl(nmac)(1:ixr-1))
!
      macdef=.true.
!
      return
!
! ****** Error: replacement text is too long.
!
  900 continue
!
      write (*,*)
      write (*,*) '### ERROR in MACDEF:'
      write (*,*) '### Macro replacement text is too long.'
      write (*,*) 'Macro number = ',nmac
      write (*,*) 'Replacement text:'
      write (*,*) macrpl(nmac)
      write (*,*) 'Input line is:'
      write (*,*) buf
      stop (1)
!
      end
!#######################################################################
subroutine filtnn (line)
!
!-----------------------------------------------------------------------
!
! ****** Filter the ## operator.
!
!-----------------------------------------------------------------------
!
      character*(*) line
!
!-----------------------------------------------------------------------
!
      external fchar
      integer fchar
!
!-----------------------------------------------------------------------
!
      ll=lenstr(line)
!
      ix=1
  100 continue
      if (ix+1.gt.ll) go to 200
      if (line(ix:ix+1).eq.'##') then
        ln=lenstr(line(1:ix-1))
        ir=max0(0,fchar(line(ix+2:))-1)
        nd=ix-1-ln+2+ir
        line(ln+1:)=line(ln+1+nd:)
        ll=ll-nd
        ix=ln+1
      else
        ix=ix+1
      end if
      go to 100
!
  200 continue
!
end subroutine
!#######################################################################
      logical function macres (line,obuf,ixo)
!
!-----------------------------------------------------------------------
!
! ****** Resolve the text in LINE for macro calls.
!
!-----------------------------------------------------------------------
!
      use params
      use macro_defs
!
!-----------------------------------------------------------------------
!
      character*(*) line,obuf
!
!-----------------------------------------------------------------------
!
      character*(LMNAM) name
!
      parameter (nargmx=20)
      character*(LMARG) args(nargmx)
      logical :: delim
!
!-----------------------------------------------------------------------
!
      logical, external :: getname
!
!-----------------------------------------------------------------------
!
      macres=.false.
!
! ****** If no macros are defined, just copy the line to the
! ****** output buffer and return.
!
      if (nmac.le.0) then
        call addtobuf (line,obuf,ixo,ierr)
        if (ierr.ne.0) go to 900
        return
      end if
!
      ix=1
!
  100 continue
!
      ix0=ix
!
      if (getname(line,ix,name,0,delim)) then
!
! ****** Write the text up to the name.
!
        ln=lenstr(name)
        call addtobuf (line(ix0:ix-1-ln),obuf,ixo,ierr)
        if (ierr.ne.0) go to 900
        ix0=ix-ln
!
! ****** Check if this is a macro call.
!
        imac=match(macnam,nmac,name)
!
        if (imac.ne.0) then
!
! ****** Get the arguments.
!
          ixhold=ix
          call getargs (line,ix,nargmx,args,narg,ierr)
          if (ierr.ne.0) then
            call addtobuf (name(1:ln),obuf,ixo,ierr)
            if (ierr.ne.0) go to 900
            go to 100
          end if
!
          if (idebug.ge.3) then
            write (*,*)
            write (*,*) '### Found a macro call:'
            write (*,*) 'Macro name = ',name
            write (*,*) 'Number of args = ',narg
            write (*,*) 'Arguments:'
            do i=1,narg
              write (*,*) 'ARG#',i,': ',args(i)
            enddo
          end if
!
! ****** Put in the replacement text.
!
          call putrpl (imac,args,narg,obuf,ixo,ierr)
          if (ierr.ne.0) then
            call addtobuf (name(1:ln),obuf,ixo,ierr)
            if (ierr.ne.0) go to 900
            ix=ixhold
            go to 100
          end if
!
          macres=.true.
!
        else
!
! ****** Write the name.
!
          call addtobuf (name(1:ln),obuf,ixo,ierr)
          if (ierr.ne.0) go to 900
!
        end if
        go to 100
!
      else
!
! ****** Write the rest of the line.
!
        call addtobuf (line(ix0:ix-1),obuf,ixo,ierr)
        if (ierr.ne.0) go to 900
!
      end if
!
      return
!
  900 continue
!
      write (*,*)
      write (*,*) '### ERROR in MACRES:'
      write (*,*) '### Output buffer overflow.'
      stop (1)
!
      end
!#######################################################################
subroutine getargs (line,ixi,nargmx,args,narg,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Get the arguments for a macro call.
!
!-----------------------------------------------------------------------
!
      character*(*) line,args(nargmx)
!
!-----------------------------------------------------------------------
!
      external find
      logical find,found
!
!-----------------------------------------------------------------------
!
      ierr=1
!
      l_arg=len(args(1))
      narg=0
      ix=ixi
!
! ****** Find the first left parenthesis.
!
      ix0=ix
      found=find(line,'(',ix)
      if (.not.found) return
!
! ****** Check that it is the first non-blank character.
!
      if (ix-1.ge.ix0.and.line(ix0:ix-1).ne.' ') return
!
      ixl=ix
      ix=ix+1
!
! ****** Find the first right parenthesis.
!
      found=find(line,')',ix)
      if (.not.found) return
!
      ixr=ix
!
! ****** Get the arguments.
!
      ix=ixl+1
  100 continue
      ix0=ix
      found=find(line(1:ixr-1),',',ix)
      if (.not.found) ix=ixr
      narg=narg+1
      if (narg.gt.nargmx) then
        write (*,*)
        write (*,*) '### ERROR in GETARGS:'
        write (*,*) '### Too many macro arguments.'
        write (*,*) 'Argument number = ',narg
        write (*,*) 'Line is:'
        write (*,*) line
        stop (1)
      end if
      larg=lenstr(line(ix0:ix-1))
      if (larg.gt.l_arg) then
        write (*,*)
        write (*,*) '### ERROR in GETARGS:'
        write (*,*) '### Macro argument is too long.'
        write (*,*) 'Argument number = ',narg
        write (*,*) 'Argument length = ',larg
        write (*,*) 'Line is:'
        write (*,*) line
        stop (1)
      end if
      args(narg)=line(ix0:ix-1)
      args(narg)=adjustl(args(narg))
      if (found) then
        ix=ix+1
        go to 100
      end if
!
! ****** Arguments found successfully.
!
      ixi=ixr+1
      ierr=0
!
end subroutine
!#######################################################################
subroutine putrpl (imac,args,narg,buf,ix,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Put the replacement text for a macro call into
! ****** the output buffer BUF.
!
! ****** The NARG actual arguments are in array ARGS.
!
! ****** IMAC is the macro name index.
!
!-----------------------------------------------------------------------
!
      use macro_defs
!
!-----------------------------------------------------------------------
!
      integer :: imac
      integer :: narg
      character*(*), dimension(narg):: args
      character*(*) :: buf
      integer :: ix
      integer :: ierr
!
!-----------------------------------------------------------------------
!
      ierr=1
!
! ****** Check the number of arguments.
!
      if (narg.ne.macnarg(imac)) return
!
! ****** Use the macro replacement text.
!
      ll=lenstr(macrpl(imac))
      ir=1
  100 continue
      if (ir.gt.ll) go to 200
!
! ****** Check if this is an argument.
!
      if (macrpl(imac)(ir:ir).eq.'$') then
!
! ****** Get the argument number.
!
        read (macrpl(imac)(ir+1:ir+2),'(i2)') iarg
        larg=lenstr(args(iarg))
!
! ****** Substitute the actual argument.
!
        call addtobuf (args(iarg)(1:larg),buf,ix,ierr)
        if (ierr.ne.0) go to 900
!
        ir=ir+3
!
      else
!
! ****** Write a character of the replacement text.
!
        call addtobuf (macrpl(imac)(ir:ir),buf,ix,ierr)
        if (ierr.ne.0) go to 900
        ir=ir+1
!
      end if
      go to 100
!
  200 continue
!
      ierr=0
!
      return
!
  900 continue
!
      write (*,*)
      write (*,*) '### ERROR in PUTRPL:'
      write (*,*) '### Output buffer overflow.'
      stop (1)
!
end subroutine
!#######################################################################
subroutine set_parameters (cname,cvers,cdate)
!
!-----------------------------------------------------------------------
!
! ****** Set parameters from the command-line arguments.
!
!-----------------------------------------------------------------------
!
      use syntax
      use paragraph_def
      use get_usage_line_interface
      use print_par_interface
      use delete_par_interface
      use params
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: cname,cvers,cdate
!
!-----------------------------------------------------------------------
!
! ****** Storage for the usage line.
!
      type(paragraph), pointer :: usage
!
! ****** Storage for the error message.
!
      character(512) :: errmsg
!
!-----------------------------------------------------------------------
!
      integer :: ierr
      character(512) :: arg
      logical :: set
!
!-----------------------------------------------------------------------
!
      integer, external :: intval
!
!-----------------------------------------------------------------------
!
! ****** Define the syntax.
!
      call defarg (GROUP_KA,'-debug','0','<level>')
      call defarg (GROUP_KA,'-macdef','<no_default>','<file>')
      call defarg (GROUP_A ,'infile',' ',' ')
      call defarg (GROUP_A ,'outfile',' ',' ')
!
! ****** Parse the command line.
!
      call parse (errmsg,ierr)
!
      if (ierr.ne.0) then
!
        write (*,*)
        write (*,*) '### ',cname,' Version ',cvers,' of ',cdate,'.'
        write (*,*) '### Expand macros in a source file.'
!
        if (ierr.gt.1) then
          write (*,*)
          write (*,*) errmsg
        end if
!
! ****** Print the usage line.
!
        call get_usage_line (usage)
!
        write (*,*)
        write (*,*) 'Usage:'
        write (*,*)
!
        call print_par (usage)
!
        write (*,*)
        write (*,*) 'The macros in <infile> are expanded, sending'// &
                    ' the output to <outfile>.'
        write (*,*)
        write (*,*) 'Use -debug to get diagnostic information:'
        write (*,*) '  debug.ge.1: lists macro definitions'
        write (*,*) '  debug.ge.2: lists macro call replacement text'
        write (*,*) '  debug.ge.3: lists every line read and'// &
                    ' detailed call diagnostic output'
        write (*,*)
        write (*,*) 'Use -macdef to specify a separate file of'// &
                    ' macro definitions.  If this file is'
        write (*,*) 'specified, it is read first, before'// &
                    ' reading the input source file.'
!
        call delete_par (usage)
!
        stop (1)
!
      end if
!
! ****** Set the parameters.
!
! ****** Debug level.
!
      call fetcharg ('-debug',set,arg)
      idebug=intval(trim(arg),'-debug')
!
! ****** Macro definitions file.
!
      call fetcharg ('-macdef',set,arg)
      if (set) then
        macro_def_file=trim(arg)
      else
        macro_def_file=' '
      end if
!
! ****** Input file name.
!
      call fetcharg ('infile',set,arg)
      infile=trim(arg)
!
! ****** Output file name.
!
      call fetcharg ('outfile',set,arg)
      outfile=trim(arg)
!
end subroutine
!
!-----------------------------------------------------------------------
!
! ****** Source to build the parsing library.
! ****** These routines are used by Zoran Mikic's tools.
!
!-----------------------------------------------------------------------
!
!        07/29/2003, ZM, Version 1.00:
!
!         - Original version of the parsing library.
!           This library was put together to facilitate the
!           development of ZM's tools.
!           It includes routines to parse the command line.
!           The code was cleaned up to use standard FORTRAN90.
!
!        01/17/2005, ZM, Version 1.01:
!
!         - Added the function NARGS_SPECIFIED to return the
!           number of arguments specified on the command line.
!
!        10/28/2005, ZM, Version 1.02:
!
!         - Added the functions LOAD_LIST_OF_REALS and
!           LOAD_LIST_OF_INTS that can be used to parse
!           arguments that contain lists of real or integer values.
!         - Changed the length of temporary arguments to equal
!           512 characters to accomodate long file names.
!
!        10/31/2006, ZM, Version 1.03:
!
!         - Removed the EXTERNAL declarations for GETARG and IARGC
!           since these are now intrinsic routines in the
!           Intel 9.1 compiler.
!
!        03/10/2008, ZM, Version 1.04:
!
!         - Added the LCASE and UCASE functions to convert strings
!           to lowercase and uppercase.
!
!-----------------------------------------------------------------------
!
!#######################################################################
module parselib_ident
!
      character(*), parameter :: cname='PARSELIB'
      character(*), parameter :: cvers='1.04'
      character(*), parameter :: cdate='03/10/2008'
!
end module
!#######################################################################
module parse_args
!
      use string_def
!
      implicit none
!
!-----------------------------------------------------------------------
! ****** Argument descriptor and storage for command-line arguments.
!-----------------------------------------------------------------------
!
! ****** Structure to hold an argument.
!
      type :: arg_descriptor
        integer :: group
        logical :: set
        logical :: required
        type(string) :: keyword
        type(string) :: name
        type(string) :: value
      end type
!
! ****** Maximum number of arguments.
!
      integer, parameter :: mxarg=100
!
! ****** Number of arguments defined.
!
      integer :: nargs
!
! ****** Argument descriptor.
!
      type(arg_descriptor), dimension(mxarg) :: args
!
! ****** Number of arguments specified.
!
      integer :: nargs_spec
!
end module
!#######################################################################
subroutine ffopen (iun,fname,mode,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Open file FNAME and link it to unit IUN.
!
!-----------------------------------------------------------------------
!
! ****** When MODE='r', the file must exist.
! ****** When MODE='w', the file is created.
! ****** When MODE='rw', the file must exist, but can be overwritten.
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      integer :: iun
      character(*) :: fname
      character(*) :: mode
      integer :: ierr
!
!-----------------------------------------------------------------------
!
      ierr=0
!
      if (mode.eq.'r') then
        open (iun,file=fname,status='old',err=900)
      else if (mode.eq.'rw') then
        open (iun,file=fname,status='replace',err=900)
      else if (mode.eq.'w') then
        open (iun,file=fname,status='new',err=900)
      else
        write (*,*)
        write (*,*) '### ERROR in FFOPEN:'
        write (*,*) '### Invalid MODE requested.'
        write (*,*) 'MODE = ',mode
        write (*,*) 'File name: ',trim(fname)
        ierr=2
        return
      end if
!
      return
!
  900 continue
!
      write (*,*)
      write (*,*) '### ERROR in FFOPEN:'
      write (*,*) '### Error while opening the requested file.'
      write (*,*) 'File name: ',trim(fname)
      write (*,*) 'MODE = ',mode
      ierr=1
!
end subroutine
!#######################################################################
function lcase (s)
!
!-----------------------------------------------------------------------
!
! ****** Convert the string S into lowercase letters and return it as
! ****** the function result.
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*), intent(in) :: s
      character(len(s)) :: lcase
!
!-----------------------------------------------------------------------
!
      integer :: i,ic
!
!-----------------------------------------------------------------------
!
      lcase=' '
!
      do i=1,len_trim(s)
        ic=iachar(s(i:i))
        if (ic.ge.65.and.ic.le.90) then
          ic=ic+32
        end if
        lcase(i:i)=achar(ic)
      end do
!
      return
end function
!#######################################################################
function ucase (s)
!
!-----------------------------------------------------------------------
!
! ****** Convert the string S into uppercase letters and return it as
! ****** the function result.
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*), intent(in) :: s
      character(len(s)) :: ucase
!
!-----------------------------------------------------------------------
!
      integer :: i,ic
!
!-----------------------------------------------------------------------
!
      ucase=' '
!
      do i=1,len_trim(s)
        ic=iachar(s(i:i))
        if (ic.ge.97.and.ic.le.122) then
          ic=ic-32
        end if
        ucase(i:i)=achar(ic)
      end do
!
      return
end function
!#######################################################################
subroutine parse (errmsg,ierr)
!
!-----------------------------------------------------------------------
!
! ****** Parse the command line.
!
!-----------------------------------------------------------------------
!
! ****** The syntax for the keyword/argument items can be defined
! ****** by using routine DEFARG.
!
! ****** On return, IERR=0 indicates that the command line was
! ****** parsed successfully.
!
! ****** IERR=1 indicates that no arguments were present.  This
! ****** is usually used to print the usage line.
!
! ****** IERR=2 indicates that a syntax error occured.
!
! ****** IERR=3 indicates that one or more required arguments
! ****** was not supplied.
!
! ****** When IERR=2 or IERR=3, an error message is put into
! ****** character string ERRMSG.
!
!-----------------------------------------------------------------------
!
      use syntax
      use parse_args
      use iso_fortran_env
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: errmsg
      integer :: ierr
!
!-----------------------------------------------------------------------
!
! ****** Command line arguments.
!
      integer :: iargc
!
!-----------------------------------------------------------------------
!
      integer, external :: matchkw
      integer, external :: nextarg
!
!-----------------------------------------------------------------------
!
      character(512) :: arg
      integer :: na,ia,ia0,iarg,ls,i
!
!-----------------------------------------------------------------------
!
! ****** Initialization.
!
      ierr=0
      nargs_spec=0
      errmsg=' '
!
! ****** Get the number of command line arguments.
!
      na=COMMAND_ARGUMENT_COUNT()
      if (na.eq.0) then
        ierr=1
        go to 900
      end if
!
      ia=1
  200 continue
!
      ia0=ia
!
! ****** Process arguments with syntax: <kw> <arg>
!
      if (na-ia+1.ge.2) then
        call GET_COMMAND_ARGUMENT (ia,arg)
        iarg=matchkw(GROUP_KA,trim(arg))
        if (iarg.gt.0) then
          if (.not.args(iarg)%set) then
            ia=ia+1
            call GET_COMMAND_ARGUMENT (ia,arg)
            call delete_str (args(iarg)%value)
            call put_str (trim(arg),args(iarg)%value)
            args(iarg)%set=.true.
            ia=ia+1
            nargs_spec=nargs_spec+1
            go to 300
          end if
        end if
      end if
!
! ****** Process arguments with syntax: <kw> <arg> <arg>
!
      if (na-ia+1.ge.3) then
        call GET_COMMAND_ARGUMENT (ia,arg)
        iarg=matchkw(GROUP_KAA,trim(arg))
        if (iarg.gt.0) then
          if (.not.args(iarg)%set) then
            ia=ia+1
            call GET_COMMAND_ARGUMENT (ia,arg)
            ls=len_trim(arg)
            ls=ls+1
            arg(ls:ls)=' '
            ia=ia+1
            call GET_COMMAND_ARGUMENT (ia,arg(ls+1:))
            call delete_str (args(iarg)%value)
            call put_str (trim(arg),args(iarg)%value)
            args(iarg)%set=.true.
            ia=ia+1
            nargs_spec=nargs_spec+1
            go to 300
          end if
        end if
      end if
!
! ****** Process arguments with syntax: <kw>
!
      if (na-ia+1.ge.1) then
        call GET_COMMAND_ARGUMENT (ia,arg)
        iarg=matchkw(GROUP_K,trim(arg))
        if (iarg.gt.0) then
          if (.not.args(iarg)%set) then
            call delete_str (args(iarg)%value)
            call put_str (' ',args(iarg)%value)
            args(iarg)%set=.true.
            ia=ia+1
            nargs_spec=nargs_spec+1
            go to 300
          end if
        end if
      end if
!
! ****** Process arguments with syntax: <arg>
!
      if (na-ia+1.ge.1) then
        iarg=nextarg(GROUP_A)
        if (iarg.gt.0) then
          call GET_COMMAND_ARGUMENT (ia,arg)
          call delete_str (args(iarg)%value)
          call put_str (trim(arg),args(iarg)%value)
          args(iarg)%set=.true.
          ia=ia+1
          nargs_spec=nargs_spec+1
          go to 300
        end if
      end if
!
  300 continue
!
! ****** Check that an argument was found.
!
      if (ia.eq.ia0) then
        ierr=2
        go to 900
      end if
!
! ****** Keep processing arguments until done.
!
      if (na-ia+1.gt.0) go to 200
!
! ****** Check that the required arguments were supplied.
!
      do i=1,nargs
        if (args(i)%required.and..not.args(i)%set) then
          ierr=3
          go to 900
        end if
      enddo
!
      return
!
! ****** Error exit.
!
  900 continue
!
      if (ierr.eq.2) then
        errmsg='### Syntax error.'
      else if (ierr.eq.3) then
        errmsg='### A required argument was not supplied.'
      end if
!
end subroutine
!#######################################################################
subroutine get_usage_line (usage)
!
!-----------------------------------------------------------------------
!
! ****** Construct the usage line in paragraph USAGE.
!
! ****** Use routine PRINT_PAR to write the usage line.
!
!-----------------------------------------------------------------------
!
      use parse_args
      use paragraph_def
      use new_par_interface
      use add_line_interface
      use get_str_interface
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: usage
!
!-----------------------------------------------------------------------
!
! ****** Right margin for printing the usage line.
!
      integer, parameter :: rmargin=78
!
!-----------------------------------------------------------------------
!
      character(512) :: line
      integer :: iarg,n0
      type(paragraph), pointer :: current_par
!
!-----------------------------------------------------------------------
!
! ****** Construct the usage line in USAGE.
!
      call new_par (usage)
      current_par=>usage
!
! ****** Start with the command name (as invoked).
!
      call GET_COMMAND_ARGUMENT (0,line)
!
      iarg=1
!
! ****** Add the arguments.
!
      do while (iarg.le.nargs)
!
! ****** Add the syntax for the next argument to LINE.
!
        n0=len_trim(line)
!
        if (args(iarg)%required) then
          line=trim(line)//' '//get_str(args(iarg)%keyword)
        else
          line=trim(line)//' ['//get_str(args(iarg)%keyword)
        end if
        line=trim(line)//' '//get_str(args(iarg)%name)
        if (.not.args(iarg)%required) then
          line=trim(line)//']'
        end if
!
! ****** Check if the addition of the argument causes the line
! ****** to wrap; if it does, break the line prior to the
! ****** argument text.
!
        if (len_trim(line).gt.rmargin) then
          call add_line (line(1:n0),current_par)
          line=' '//line(n0+1:)
        end if
!
! ****** If the line is still too long, force a break at RMARGIN
! ****** until the line is shorter than RMARGIN.
!
        do while (len_trim(line).gt.rmargin)
          call add_line (line(1:rmargin),current_par)
          line='  '//line(rmargin+1:)
        enddo
!
! ****** Process the next argument.
!
        iarg=iarg+1
!
      enddo
!
! ****** Add the last line to the paragraph.
!
      if (line.ne.' ') call add_line (trim(line),current_par)
!
end subroutine
!#######################################################################
subroutine defarg (group,keyword,default,name)
!
!-----------------------------------------------------------------------
!
! ****** Define the syntax for a command line argument item.
!
!-----------------------------------------------------------------------
!
! ****** GROUP is the syntax group index;
! ****** KEYWORD is the keyword;
! ****** DEFAULT is the default value of the argument;
! ****** NAME is the name of the argument (for use in
! ****** constructing the usage line).
!
!-----------------------------------------------------------------------
!
      use syntax
      use parse_args
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      integer :: group
      character(*) :: keyword
      character(*) :: default
      character(*) :: name
!
!-----------------------------------------------------------------------
!
! ****** Check that the group index is valid.
!
      if (group.lt.0.or.group.gt.ngroups) then
        write (*,*)
        write (*,*) '### ERROR in DEFARG:'
        write (*,*) '### An invalid group index was specified.'
        write (*,*) 'Group index = ',group
        write (*,*) 'Keyword = ',trim(keyword)
        if (name.ne.' ') write (*,*) 'Name = ',trim(name)
        write (*,*)
        write (*,*) '### This indicates a programming error'// &
                    ' in the syntax definition and use.'
        stop (1)
      end if
!
! ****** Check for a null keyword.
!
      if (keyword.eq.' ') then
        write (*,*)
        write (*,*) '### ERROR in DEFARG:'
        write (*,*) '### The keyword is null.'
        write (*,*) 'Group index = ',group
        if (name.ne.' ') write (*,*) 'Name = ',trim(name)
        write (*,*)
        write (*,*) '### This indicates a programming error'// &
                    ' in the syntax definition and use.'
        stop (1)
      end if
!
! ****** Increment the argument counter.
!
      if (nargs.ge.mxarg) then
        write (*,*)
        write (*,*) '### ERROR in DEFARG:'
        write (*,*) '### Exceeded the number of allowed arguments.'
        write (*,*) 'Maximum number of arguments = ',mxarg
        write (*,*) 'Group index = ',group
        write (*,*) 'Keyword = ',trim(keyword)
        if (name.ne.' ') write (*,*) 'Name = ',trim(name)
        write (*,*)
        write (*,*) '### This indicates a programming error'// &
                    ' in the syntax definition and use.'
        stop (1)
      end if
!
      nargs=nargs+1
!
! ****** Store the group index and keyword.
!
! ****** For group GROUP_A (single arguments), the name of the
! ****** argument is passed as the "keyword".
!
      args(nargs)%group=group
      call put_str (trim(keyword),args(nargs)%keyword)
!
! ****** Initialize the flag that indicates whether an argument
! ****** has been set.
!
      args(nargs)%set=.false.
!
! ****** If a default argument was supplied, the argument
! ****** does not have to be set.  Use DEFAULT=' ' to
! ****** indicate that an argument is required.
!
! ****** If a default argument has been supplied, store it in
! ****** ARGS(nargs)%VALUE.  If there is no default,
! ****** set ARGS(nargs)%VALUE to an empty string.
!
! ****** Since group GROUP_K doesn't have an argument,
! ****** DEFAULT is ignored for this group.
!
      if (group.eq.GROUP_K) then
        args(nargs)%required=.false.
        call put_str (' ',args(nargs)%value)
      else
        if (default.eq.' ') then
          args(nargs)%required=.true.
          call put_str (' ',args(nargs)%value)
        else
          args(nargs)%required=.false.
          call put_str (trim(default),args(nargs)%value)
        end if
      end if
!
! ****** Store the argument name.  For groups GROUP_K (keywords)
! ****** and GROUP_A (single arguments), there is no argument name,
! ****** so NAME is ignored.
!
      if (group.eq.GROUP_K.or.group.eq.GROUP_A) then
        call put_str (' ',args(nargs)%name)
      else
        call put_str (trim(name),args(nargs)%name)
      end if
!
end subroutine
!#######################################################################
subroutine fetcharg (keyword,set,arg)
!
!-----------------------------------------------------------------------
!
! ****** Fetch the value of the argument corresponding to
! ****** keyword KEYWORD.
!
!-----------------------------------------------------------------------
!
! ****** If KEYWORD is a keyword-type argument (GROUP_K), return
! ****** its setting through variable SET.  The variable ARG should
! ****** be ignored for this type of keyword.
!
! ****** For keywords with arguments (GROUP_A, GROUP_KA, and
! ****** GROUP_KAA), return the value of the arguments in ARG,
! ****** and return SET=.true. if they were set via the command line;
! ****** otherwise, return SET=.false..
!
!-----------------------------------------------------------------------
!
      use parse_args
      use get_str_interface
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: keyword
      logical :: set
      character(*) :: arg
!
!-----------------------------------------------------------------------
!
      integer :: i
!
!-----------------------------------------------------------------------
!
      do i=nargs,1,-1
        if (keyword.eq.get_str(args(i)%keyword)) go to 100
      enddo
!
      write (*,*)
      write (*,*) '### ERROR in FETCHARG:'
      write (*,*) '### The requested keyword could not be matched.'
      write (*,*) 'Keyword = ',trim(keyword)
      write (*,*)
      write (*,*) '### This indicates a programming error'// &
                  ' in the syntax definition and use.'
      stop (1)
!
  100 continue
!
      set=args(i)%set
      arg=get_str(args(i)%value)
!
end subroutine
!#######################################################################
function matchkw (group,keyword)
!
!-----------------------------------------------------------------------
!
! ****** Match keyword KEYWORD against the list of keywords in
! ****** group GROUP.
!
! ****** If found, set the function value to the corresponding
! ****** argument number.  Otherwise, return MATCHKW=0.
!
!-----------------------------------------------------------------------
!
      use parse_args
      use get_str_interface
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      integer :: group
      character(*) :: keyword
      integer :: matchkw
!
!-----------------------------------------------------------------------
!
      integer :: i
!
!-----------------------------------------------------------------------
!
      matchkw=0
!
      do i=nargs,1,-1
        if (group.eq.args(i)%group) then
          if (keyword.eq.get_str(args(i)%keyword)) then
            matchkw=i
            return
          end if
        end if
      enddo
!
      return
end function
!#######################################################################
function nextarg (group)
!
!-----------------------------------------------------------------------
!
! ****** Find the position of the next argument in group GROUP
! ****** that has not been set.
!
!-----------------------------------------------------------------------
!
! ****** If an empty slot is found, set the function value
! ****** to the corresponding argument number.
!
! ****** Otherwise, return NXTARG=0.
!
!-----------------------------------------------------------------------
!
      use parse_args
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      integer :: group
      integer :: nextarg
!
!-----------------------------------------------------------------------
!
      integer :: i
!
!-----------------------------------------------------------------------
!
      nextarg=0
!
      do i=1,nargs
        if (group.eq.args(i)%group) then
          if (.not.args(i)%set) then
            nextarg=i
            return
          end if
        end if
      enddo
!
      return
end function
!#######################################################################
subroutine nargs_specified (n)
!
!-----------------------------------------------------------------------
!
! ****** Return the number of arguments specified on the command
! ****** line.
!
!-----------------------------------------------------------------------
!
      use parse_args
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      integer :: n
!
!-----------------------------------------------------------------------
!
      n=nargs_spec
!
end subroutine
!#######################################################################
subroutine new_par (par)
!
!-----------------------------------------------------------------------
!
! ****** Initialize paragraph PAR.
!
!-----------------------------------------------------------------------
!
      use paragraph_def
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: par
!
!-----------------------------------------------------------------------
!
      allocate (par)
      nullify (par%line%c)
      nullify (par%next)
!
end subroutine
!#######################################################################
subroutine delete_par (par)
!
!-----------------------------------------------------------------------
!
! ****** Delete paragraph PAR and deallocate its storage and that
! ****** of its linked lists.
!
!-----------------------------------------------------------------------
!
      use paragraph_def
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: par
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: current_par,previous_par
!
!-----------------------------------------------------------------------
!
      current_par=>par
!
      do
!
! ****** Deallocate the line buffer.
!
        call delete_str (current_par%line)
!
! ****** Set the pointer to the next line (if it has been defined).
!
        if (.not.associated(current_par%next)) exit
        previous_par=>current_par
        current_par=>current_par%next
        deallocate (previous_par)
!
      enddo
!
      deallocate (current_par)
!
end subroutine
!#######################################################################
subroutine add_line (line,par)
!
!-----------------------------------------------------------------------
!
! ****** Add LINE to paragraph PAR.
!
! ****** On exit from this routine, PAR points to a new line,
! ****** and can be used to store the next line of text.
!
!-----------------------------------------------------------------------
!
      use paragraph_def
      use new_par_interface
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: line
      type(paragraph), pointer :: par
!
!-----------------------------------------------------------------------
!
! ****** Store LINE into the string buffer for the current line.
!
      call put_str (line,par%line)
!
! ****** Allocate a pointer to the next line.
!
      call new_par (par%next)
!
! ****** Set PAR to point to the next line.
!
      par=>par%next
!
end subroutine
!#######################################################################
subroutine print_par (par)
!
!-----------------------------------------------------------------------
!
! ****** Print all lines of paragraph PAR to STDOUT.
!
!-----------------------------------------------------------------------
!
      use paragraph_def
      use get_str_interface
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: par
!
!-----------------------------------------------------------------------
!
      type(paragraph), pointer :: current_par
!
!-----------------------------------------------------------------------
!
      current_par=>par
!
      do
!
! ****** Print the line if it has been defined.
!
        if (associated(current_par%line%c)) then
          write (*,*) trim(get_str(current_par%line))
        end if
!
! ****** Set the pointer to the next line (if it has been defined).
!
        if (.not.associated(current_par%next)) exit
        current_par=>current_par%next
!
      enddo
!
end subroutine
!#######################################################################
subroutine put_str (cval,str)
!
!-----------------------------------------------------------------------
!
! ****** Store character variable CVAL into string STR.
! ****** This routine allocates storage for the string.
!
!-----------------------------------------------------------------------
!
      use string_def
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: cval
      type(string) :: str
!
!-----------------------------------------------------------------------
!
      integer :: l,i
!
!-----------------------------------------------------------------------
!
      l=len(cval)
!
      allocate (str%c(l))
!
      do i=1,l
        str%c(i)=cval(i:i)
      enddo
!
end subroutine
!#######################################################################
function get_str (str)
!
!-----------------------------------------------------------------------
!
! ****** Return the value of string STR as the function value
! ****** (as an assumed-length character variable).
!
!-----------------------------------------------------------------------
!
      use string_def
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(string) :: str
      character(size(str%c)) :: get_str
!
!-----------------------------------------------------------------------
!
      integer :: i
!
!-----------------------------------------------------------------------
!
      do i=1,size(str%c)
        get_str(i:i)=str%c(i)
      enddo
!
      return
end function
!#######################################################################
subroutine delete_str (str)
!
!-----------------------------------------------------------------------
!
! ****** Delete the storage for string STR.
!
!-----------------------------------------------------------------------
!
      use string_def
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      type(string) :: str
!
!-----------------------------------------------------------------------
!
      if (associated(str%c)) then
        deallocate (str%c)
      end if
      nullify (str%c)
!
end subroutine
!#######################################################################
function intval (avalue,name)
!
!-----------------------------------------------------------------------
!
! ****** Get the value of the integer in character variable AVALUE.
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: avalue
      character(*) :: name
      integer :: intval
!
!-----------------------------------------------------------------------
!
      logical, external :: ifint
!
!-----------------------------------------------------------------------
!
      integer :: ivalue
!
!-----------------------------------------------------------------------
!
      if (.not.ifint(trim(avalue),ivalue)) then
        write (*,*)
        write (*,*) '### ERROR in INTVAL:'
        write (*,*) '### Could not interpret an integer '// &
                    'while setting: ',trim(name)
        write (*,*) 'Invalid format: ',trim(avalue)
        stop (1)
      end if
!
      intval=ivalue
!
      return
end function
!#######################################################################
function fpval (avalue,name)
!
!-----------------------------------------------------------------------
!
! ****** Get the value of the floating point number in character
! ****** variable AVALUE.
!
!-----------------------------------------------------------------------
!
      use number_types
!
!-----------------------------------------------------------------------
!
      character(*) :: avalue
      character(*) :: name
      real(r_typ) :: fpval
!
!-----------------------------------------------------------------------
!
      logical, external :: iffp
!
!-----------------------------------------------------------------------
!
      real(r_typ) :: value
!
!-----------------------------------------------------------------------
!
      if (.not.iffp(trim(avalue),value)) then
        write (*,*)
        write (*,*) '### ERROR in FPVAL:'
        write (*,*) '### Could not interpret a floating point '// &
                    'number while setting: ',trim(name)
        write (*,*) 'Invalid format: ',trim(avalue)
        stop (1)
      end if
!
      fpval=value
!
      return
end function
!#######################################################################
function iffp (alpha,value)
!
!-----------------------------------------------------------------------
!
! ****** Determine if ALPHA represents a floating point number;
! ****** if so, return its value in VALUE.
!
!-----------------------------------------------------------------------
!
! ****** Set IFFP=.TRUE. if ALPHA contains an alphanumeric
! ****** string with the following format:
!
!       ALPHA = '[A][B...B][.][B...B][e[A]B[B...B]]',
!
! ****** where A represents a + or - sign, and B represents a digit
! ****** between 0 and 9, inclusive.
! ****** The exponent may be denoted by a lower or upper case e.
! ****** The mantissa must have at least one digit, and the
! ****** the exponent, if present, must have between 1 and 3 digits.
! ****** Otherwise, set IFFP=.FALSE.
!
!-----------------------------------------------------------------------
!
      use number_types
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: alpha
      real(r_typ) :: value
      logical :: iffp
!
!-----------------------------------------------------------------------
!
      integer :: nmant,nexp,k,i,ke
      logical :: ifpoint,ifexp
      character(7) :: fmt
!
!-----------------------------------------------------------------------
!
      iffp=.false.
      ifpoint=.false.
      ifexp=.false.
      nmant=0
      nexp=0
!
      do k=1,len_trim(alpha)
        i=iachar(alpha(k:k))
!
! ****** Check for a sign in the first position.
!
        if (k.eq.1.and.(i.eq.43.or.i.eq.45)) cycle
!
! ****** Check for a digit.
!
        if (i.ge.48.and.i.le.57) then
!
! ****** Count digits in mantissa and exponent.
!
        if (ifexp) then
          nexp=nexp+1
          else
            nmant=nmant+1
          end if
          cycle
!
        end if
!
! ****** Check for a decimal point.
!
        if (.not.ifpoint.and.i.eq.46) then
!
! ****** Check that we are in the mantissa.
!
          if (.not.ifexp) then
            ifpoint=.true.
            cycle
          end if
!
        end if
!
! ****** Check for an exponent.
!
        if (.not.ifexp.and.(i.eq.101.or.i.eq.69)) then
          ifexp=.true.
          ke=k
          cycle
        end if
!
! ****** Check for an exponent sign.
!
        if (ifexp.and.k.eq.ke+1.and.(i.eq.43.or.i.eq.45)) cycle
!
! ****** Failed check: fall through here.
!
        iffp=.false.
!
        return
!
      enddo
!
! ****** Final check of validity: check number of digits in
! ****** the mantissa and exponent.
!
      if (nmant.ge.1) iffp=.true.
      if (ifexp.and.(nexp.lt.1.or.nexp.gt.3)) iffp=.false.
!
! ****** Obtain its numeric value.
!
      fmt='(f  .0)'
      write (fmt(3:4),'(i2.2)') len_trim(alpha)
!
      if (iffp) read (alpha,fmt) value
!
      return
end function
!#######################################################################
function ifint (alpha,ivalue)
!
!-----------------------------------------------------------------------
!
! ****** If ALPHA represents an integer, return IFINT=.true., and
! ****** put its value into IVALUE.
!
! ****** Otherwise, return IFINT=.false..
!
!-----------------------------------------------------------------------
!
! ****** A valid integer has the format:
!
!          ALPHA = '[A]B[B...B]',
!
! ****** where A represents a + or - sign, and B represents a digit
! ****** between 0 and 9, inclusive.
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: alpha
      integer :: ivalue
      logical :: ifint
!
!-----------------------------------------------------------------------
!
      integer :: k,i
      character(5) :: fmt
!
!-----------------------------------------------------------------------
!
      ifint=.false.
!
      do k=1,len_trim(alpha)
!
        i=iachar(alpha(k:k))
!
! ****** Check for a sign in the first position.
!
        if (k.eq.1.and.(i.eq.43.or.i.eq.45)) cycle
!
! ****** Check for a digit.
!
        if (i.ge.48.and.i.le.57) then
          ifint=.true.
          cycle
        end if
!
! ****** Failed check: fall through here.
!
        ifint=.false.
!
        return
!
      enddo
!
! ****** Obtain its numeric value.
!
      fmt='(i  )'
      write (fmt(3:4),'(i2.2)') len_trim(alpha)
!
      if (ifint) read (alpha,fmt) ivalue
!
      return
end function
!#######################################################################
subroutine load_list_of_reals (s,label,n,f)
!
!-----------------------------------------------------------------------
!
! ****** Read N real values from character string S into
! ****** array F(N). The values in S may be either space or
! ****** comma separated.
!
!-----------------------------------------------------------------------
!
      use number_types
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: s
      character(*) :: label
      integer :: n
      real(r_typ), dimension(n) :: f
      intent(in) :: s,label,n
      intent(out) :: f
!
!-----------------------------------------------------------------------
!
      integer :: i,i0,i1
      character :: delimiter
      character(512) :: list
!
!-----------------------------------------------------------------------
!
      real(r_typ), external :: fpval
!
!-----------------------------------------------------------------------
!
! ****** Make a local copy of the string (removing leading spaces).
!
      list=adjustl(s)
!
! ****** If any commas are present, use a comma as the delimiter.
! ****** Otherwise, one or more spaces is used as a delimiter.
! ****** In this case, compress multiple spaces into a single space.
!
      if (index(list,',').ne.0) then
        delimiter=','
      else
        delimiter=' '
        call delete_repeated_char (list,' ')
      end if
!
! ****** Read the list of N numbers sequentially into F.
!
      i0=1
      do i=1,n-1
        i1=scan(list(i0:),delimiter)+i0-2
        f(i)=fpval(adjustl(list(i0:i1)),label)
        i0=i1+2
      enddo
      f(n)=fpval(adjustl(list(i0:)),label)
!
end subroutine
!#######################################################################
subroutine load_list_of_ints (s,label,n,j)
!
!-----------------------------------------------------------------------
!
! ****** Read N integer values from character string S into
! ****** array J(N).  The values in S may be either space or
! ****** comma separated.
!
!-----------------------------------------------------------------------
!
      use number_types
!
!-----------------------------------------------------------------------
!
      implicit none
!
!-----------------------------------------------------------------------
!
      character(*) :: s
      character(*) :: label
      integer :: n
      integer, dimension(n) :: j
      intent(in) :: s,label,n
      intent(out) :: j
!
!-----------------------------------------------------------------------
!
      integer :: i,i0,i1
      character :: delimiter
      character(512) :: list
!
!-----------------------------------------------------------------------
!
      integer, external :: intval
!
!-----------------------------------------------------------------------
!
! ****** Make a local copy of the string (removing leading spaces).
!
      list=adjustl(s)
!
! ****** If any commas are present, use a comma as the delimiter.
! ****** Otherwise, one or more spaces is used as a delimiter.
! ****** In this case, compress multiple spaces into a single space.
!
      if (index(list,',').ne.0) then
        delimiter=','
      else
        delimiter=' '
        call delete_repeated_char (list,' ')
      end if
!
! ****** Read the list of N numbers sequentially into J.
!
      i0=1
      do i=1,n-1
        i1=scan(list(i0:),delimiter)+i0-2
        j(i)=intval(adjustl(list(i0:i1)),label)
        i0=i1+2
      enddo
      j(n)=intval(adjustl(list(i0:)),label)
!
end subroutine
!#######################################################################
subroutine delete_repeated_char (s,c)
!
!-----------------------------------------------------------------------
!
! ****** Transform repeated adjoining occurrences of character C
! ****** in string S into single occurrences of C.
!
! ****** The string S is overwritten by the modified string.
!
! ****** Trailing blanks in S are ignored.
!
!-----------------------------------------------------------------------
!
! ****** For example, suppose this routine is called with C='d' and
! ****** S='abcdddeefdhdd'.  On return, S will have the value
! ****** 'abcdeefdhd'.
!
!-----------------------------------------------------------------------
!
! ****** This routine uses the FORTRAN90 intrinsic SCAN.
!
!-----------------------------------------------------------------------
!
      character(*) :: s
      character :: c
      intent(in) :: c
      intent(inout) :: s
!
!-----------------------------------------------------------------------
!
      integer :: i,i0
!
!-----------------------------------------------------------------------
!
      i0=1
      do
        i=scan(trim(s(i0:)),c)
        if (i.eq.0) exit
        i0=i0+i
        do
          if (s(i0:i0).ne.c) exit
          s(i0:)=s(i0+1:)
        enddo
      enddo
!
end subroutine
