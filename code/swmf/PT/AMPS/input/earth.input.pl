#!/usr/bin/perl
#$Id$
#reads the "Exosphere" section of the input file and modify the exosphere model of the PIC code

use strict;
use warnings;
use Cwd;
use POSIX qw(strftime);
use List::Util qw(first);
use Cwd qw(cwd);

use lib cwd;
use ampsConfigLib;


#my $command= $];
#print "Perl version : ".$command;



#arguments: 
#$ARGV[0] -> the name of the intput file 
#$ARGV[1] -> the working directory


#print "Process the exospehre model input:\n";


my $InputFileName=$ARGV[0]; #"europa.input.Assembled.Block"; #$ARGV[0];  # moon.input.Assembled.Block";
my $SpeciesFileName=$InputFileName; $SpeciesFileName =~ s/\.Block$/.Species/;
my $WorkingSourceDirectory=$ARGV[1];   #"srcTemp"; #$ARGV[1];   # srcTemp

$ampsConfigLib::WorkingSourceDirectory=$WorkingSourceDirectory;

my $line;
my $LineOriginal;

my $InputFileLineNumber=0;
my $FileName;
my $InputLine;
my $InputComment;
my $s0;
my $s1;
my $s2;


#get the list of the speces 
my $TotalSpeciesNumber;
my @SpeciesList;

open (SPECIES,"<$SpeciesFileName") || die "Cannot open file $SpeciesFileName\n";
$TotalSpeciesNumber=<SPECIES>;
@SpeciesList=<SPECIES>;
close (SPECIES);
      
chomp($TotalSpeciesNumber);
foreach (@SpeciesList) {
  chomp($_);
}


#add the model header to the pic.h
open (PIC_H,">>$WorkingSourceDirectory/pic/pic.h") || die "Cannot open $WorkingSourceDirectory/pic/pic.h";
close (PIC_H);

open (InputFile,"<","$InputFileName") || die "Cannot find file \"$InputFileName\"\n";

while ($line=<InputFile>) {
  ($InputFileLineNumber,$FileName)=split(' ',$line);
  $line=<InputFile>;
  
  $LineOriginal=$line;
  chomp($line);
  
  
  ($InputLine,$InputComment)=split('!',$line,2);
  $InputLine=uc($InputLine);
  chomp($InputLine);
 
  $InputLine=~s/[:,;=]/ /g;
  ($InputLine,$InputComment)=split(' ',$InputLine,2);
  $InputLine=~s/ //g;
  
  if ($InputLine eq "MESHSIGNATURE") {
    $line=~s/[=():]/ /g;
    ($InputLine,$line)=split(' ',$line,2);
    ($InputLine,$line)=split(' ',$line,2);

    ampsConfigLib::ChangeValueOfVariable("char Earth::Mesh::sign\\[_MAX_STRING_LENGTH_PIC_\\]","\"".$InputLine."\"","main/Earth.cpp");
  }
  elsif ($InputLine eq "SEP") {
    ($s0,$s1)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_SEP__MODE_","_PIC_MODE_ON_","main/Earth.dfn");
    } 
    elsif($s0 eq "OFF") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_SEP__MODE_","_PIC_MODE_OFF_","main/Earth.dfn");
    }
    else {
      die "Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }    
  }
  elsif ($InputLine eq "GCR") {
    ($s0,$s1)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_GCR__MODE_","_PIC_MODE_ON_","main/Earth.dfn");
    } 
    elsif($s0 eq "OFF") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_GCR__MODE_","_PIC_MODE_OFF_","main/Earth.dfn");
    }
    else {
      die "Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }     
  }
  elsif ($InputLine eq "SW") {
    ($s0,$s1)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_SW__MODE_","_PIC_MODE_ON_","main/Earth.dfn");
    }
    elsif($s0 eq "OFF") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_SW__MODE_","_PIC_MODE_OFF_","main/Earth.dfn");
    }
    else {
      die "Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  }
  elsif ($InputLine eq "ELECTRON") {
    ($s0,$s1)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_ELECTRON__MODE_","_PIC_MODE_ON_","main/Earth.dfn");
    }
    elsif($s0 eq "OFF") {
      ampsConfigLib::RedefineMacro("_PIC_EARTH_ELECTRON__MODE_","_PIC_MODE_OFF_","main/Earth.dfn");
    }
    else {
      die "Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  }
  
  #sampling of the gyro-radius and gyro-frecuency in the computational domain
  elsif ($InputLine eq "PARTICLEDATASAMPLINGMODE") {
    ($s0,$InputComment)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::ParticleData::SamplingMode","true","main/Earth_Sampling.cpp");      
      ($s0,$InputComment)=split(' ',$InputComment,2);
      
      if ($s0 eq "FUNCTION") {        
        ($s1,$s0)=split('!',$line,2);
        $s1=~s/[,;=]/ /g;
        
        ($s0,$s1)=split(' ',$s1,2);             
        ($s0,$s1)=split(' ',$s1,2);                
        ($s0,$s1)=split(' ',$s1,2);        
        ($s0,$s1)=split(' ',$s1,2);
                
        ampsConfigLib::AddLine2File("#ifdef _PIC_USER_DEFING_PARTICLE_SAMPLING__NODE_ \n#undef _PIC_USER_DEFING_PARTICLE_SAMPLING__NODE_ \n#endif \n\n#define _PIC_USER_DEFING_PARTICLE_SAMPLING__NODE_(tempParticleData,LocalParticleWeight,tempSamplingBuffer,s,node)   ".$s0."(tempParticleData,LocalParticleWeight,tempSamplingBuffer,s,node)\n","pic/picGlobal.dfn");        
      }
      else {
        die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
      }        
    }
    elsif ($s0 eq "OFF") {
      ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::ParticleData::SamplingMode","false","main/Earth_Sampling.cpp");
    }
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  }
  
  #sample the phase space  of injected particles at the boundary of the domain that can reach the near Earth region
  elsif ($InputLine eq "SAMPLEINJECTIONPHASESPACE") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON")  {
      ampsConfigLib::ChangeValueOfVariable("bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::ActiveFlag","true","main/DomainBoundaryParticlePropertyTable.cpp");
      
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
      
        if ($s0 eq "NOUTPUTCYCLES") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleNumber",$s0,"main/DomainBoundaryParticlePropertyTable.cpp");      
        }
        elsif ($s0 eq "OCCUPIEDPHASESPACETABLEFRACTION") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::OccupiedPhaseSpaceTableFraction",$s0,"main/DomainBoundaryParticlePropertyTable.cpp");               
        }
        elsif ($s0 eq "RESETPARTICLEBUFFER") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          
          if ($s0 eq "ON") {
            ampsConfigLib::ChangeValueOfVariable("bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleResetParticleBuffer","true","main/DomainBoundaryParticlePropertyTable.cpp"); 
          }
          elsif ($s0 eq "OFF") {
            ampsConfigLib::ChangeValueOfVariable("bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::LastActiveOutputCycleResetParticleBuffer","false","main/DomainBoundaryParticlePropertyTable.cpp");
          } 
          else {
            warn("Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled");
            die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
          }
        }
        else {
          warn("Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled");
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }     
      }
    }
    elsif ($s0 eq "OFF") {
      ampsConfigLib::ChangeValueOfVariable("bool Earth::CutoffRigidity::DomainBoundaryParticleProperty::SamplingParameters::ActiveFlag","false","main/DomainBoundaryParticlePropertyTable.cpp");
    }
    else {
      warn("Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled");
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  } 

  #the directionof the newly created particles
  elsif ($InputLine eq "INJECTIONPARTICLEVELOCITYDIRECTION") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "UNIFORM")  {
      ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::ParticleVelocityDirectionMode","Earth::CutoffRigidity::ParticleVelocityDirectionUniform","main/CutoffRigidity.cpp");
    }
    elsif ($s0 eq "VERTICAL") {
      ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::ParticleVelocityDirectionMode","Earth::CutoffRigidity::ParticleVelocityDirectionVertical","main/CutoffRigidity.cpp");
    }
    else {
      warn("Cannot recognize line $InputFileLineNumber ($line) in $InputFileName.Assembled");
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  }
 
  
  #the number, locations, energy range, and the number of the energy intervals used in the spherical sampling surfaces 
  elsif ($InputLine eq "SPHERICALSHELLS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON")  {
      my @ShellRadiiTable;
      my $ShellRadiiTableLength=0;
      
      #sampling will occurs
      ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::SamplingMode","true","main/Earth_Sampling.cpp");
      
      #read the rest of the line
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
      
        #cgeck wether the entry is the list of the shells radii
        if ($s0 eq "X") {
          while (defined $InputComment) {
            ($s0,$InputComment)=split(' ',$InputComment,2);
            
            if ( ($s0 eq "EMIN") || ($s0 eq "EMAX") || ($s0 eq "NLEVELS") ) {
              #the entry is a keyword of the section -> exist from the 'if statment' and parse the rest of the line
              last;
            }
            else {
              #the entry seems to be the next location of the samplign shell -> add it the list of the radii
              push(@ShellRadiiTable,$s0);
              $ShellRadiiTableLength++;
            }            
          }
          
          #the list of the shell's radii is complete -> add it to the sources 
          ampsConfigLib::ChangeValueOfArray("double Earth::Sampling::SampleSphereRadii\\[Earth::Sampling::nSphericalShells\\]",\@ShellRadiiTable,"main/Earth_Sampling.cpp");
          ampsConfigLib::ChangeValueOfVariable("    const int nSphericalShells",$ShellRadiiTableLength,"main/Earth.h");                   
        }
        
        #check whether the entry is another setting parameter
        if ($s0 eq "EMIN") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::Sampling::Fluency::minSampledEnergy",$s0,"main/Earth.cpp");         
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "EMAX") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::Sampling::Fluency::maxSampledEnergy",$s0,"main/Earth.cpp");          
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "NLEVELS") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::Sampling::Fluency::nSampledLevels",$s0,"main/Earth.cpp");
        }
        elsif ($s0 eq "NTOTALPARTICLES") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "NINJECTIONITERATIONS") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations",$s0,"main/CutoffRigidity.cpp");
        }
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }       
      }      
    }
    elsif ($s0 eq "OFF") {
       ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::SamplingMode","false","main/Earth_Sampling.cpp");
    }     
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }  
  }
  
  #energy spectrum of SEPs
  elsif ($InputLine eq "SEPENERGYSPECTRUMFILE") {
    $line=~s/[(=)]/ /g;

    ($InputLine,$line)=split(' ',$line,2); 
    ($InputLine,$line)=split(' ',$line,2); 

    ampsConfigLib::ChangeValueOfVariable("string Earth::BoundingBoxInjection::SEP::SepEnergySpecrumFile","\"".$InputLine."\"","main/BoundaryInjection_SEP.cpp");
 }

  #model mode: boundary injection vs cutoff rigidity calculation
  elsif ($InputLine eq "MODELMODE") {
    ($s0,$InputComment)=split(' ',$InputComment,2);

    if ($s0 eq "RIGIDITYCUTOFF") {
      ampsConfigLib::ChangeValueOfVariable("int Earth::ModelMode","Earth::CutoffRigidityMode","main/Earth.cpp");
    }
    elsif ($s0 eq "BOUNDARYINJECTION") {
      ampsConfigLib::ChangeValueOfVariable("int Earth::ModelMode","Earth::BoundaryInjectionMode","main/Earth.cpp"); 
    }
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
  }
  
  #sample rigidity, density, flux, and energy spectrum in individual locations
  elsif ($InputLine eq "SAMPLEINDIVIDUALPOINTS") {
      $InputComment=~s/[()]/ /g;
 
      my $cnt=0;
      my ($s0,$s1);

      $s1=$line; 
      $s1=~s/[(=)]/ /g;

      while (defined $InputComment) {
        ($InputLine,$InputComment)=split(' ',$InputComment,2);
        $cnt++;

        $InputLine=uc($InputLine);

        if ($InputLine eq "OFF") {
          last;
        }
        if ($InputLine eq "ON") {
        }
        elsif ($InputLine eq "FILE") {
          ($InputLine,$InputComment)=split(' ',$InputComment,2);
          $cnt++; 

          for (my $ii=0;$ii<$cnt;$ii++) {
            ($s0,$s1)=split(' ',$s1,2);
          }

          ($s0,$s1)=split(' ',$s1,2);
          ampsConfigLib::ChangeValueOfVariable("string Earth::IndividualPointSample::fname","\"".$s0."\"","main/SamplingIndividualPoints.cpp");
        }
        elsif ($InputLine eq "EMIN") {
          ($InputLine,$InputComment)=split(' ',$InputComment,2);
          $cnt++; 

          ampsConfigLib::ChangeValueOfVariable("double Earth::IndividualPointSample::Emin",$InputLine,"main/SamplingIndividualPoints.cpp");
        }
        elsif ($InputLine eq "EMAX") {
          ($InputLine,$InputComment)=split(' ',$InputComment,2);
          $cnt++; 

          ampsConfigLib::ChangeValueOfVariable("double Earth::IndividualPointSample::Emax",$InputLine,"main/SamplingIndividualPoints.cpp");
        }
        elsif ($InputLine eq "SCALE") {
          ($InputLine,$InputComment)=split(' ',$InputComment,2);
          $cnt++; 

          if ($InputLine eq "LINEAR") {
            ampsConfigLib::ChangeValueOfVariable("int Earth::IndividualPointSample::SamplingMode","Earth::IndividualPointSample::ModeLinear","main/SamplingIndividualPoints.cpp");
          }
          elsif ($InputLine eq "LOGARITHMIC") {
            ampsConfigLib::ChangeValueOfVariable("int Earth::IndividualPointSample::SamplingMode","Earth::IndividualPointSample::ModeLogarithmic","main/SamplingIndividualPoints.cpp");
          }
        }
        elsif ($InputLine eq "NSAMPLEINTERVALS") {
          ($InputLine,$InputComment)=split(' ',$InputComment,2);
          $cnt++;

           ampsConfigLib::ChangeValueOfVariable("int Earth::IndividualPointSample::nSampleIntervals",$InputLine,"main/SamplingIndividualPoints.cpp");
        }
         else {
           die "Cannot recognize $InputLine, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }
     }
   } 
  
  
    #the number, locations, energy range, and the number of the energy intervals used in the spherical sampling surfaces 
  elsif ($InputLine eq "SAMPLINGSPHERICALSHELLS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON")  {
      my @ShellRadiiTable;
      my $ShellRadiiTableLength=0;
      
      #sampling will occurs
      ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::SamplingMode","true","main/Earth_Sampling.cpp");
      
      #read the rest of the line
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
      
        #cgeck wether the entry is the list of the shells radii
        if ($s0 eq "X") {
          while (defined $InputComment) {
            ($s0,$InputComment)=split(' ',$InputComment,2);
            
            if ( ($s0 eq "EMIN") || ($s0 eq "EMAX") || ($s0 eq "NLEVELS") ) {
              #the entry is a keyword of the section -> exist from the 'if statment' and parse the rest of the line
              last;
            }
            else {
              #the entry seems to be the next location of the samplign shell -> add it the list of the radii
              push(@ShellRadiiTable,$s0);
              $ShellRadiiTableLength++;
            }            
          }
          
          #the list of the shell's radii is complete -> add it to the sources 
          ampsConfigLib::ChangeValueOfArray("double Earth::Sampling::SampleSphereRadii\\[Earth::Sampling::nSphericalShells\\]",\@ShellRadiiTable,"main/Earth_Sampling.cpp");
          ampsConfigLib::ChangeValueOfVariable("    const int nSphericalShells",$ShellRadiiTableLength,"main/Earth.h");                   
        }
        
        #check whether the entry is another setting parameter
        if ($s0 eq "EMIN") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::Sampling::Fluency::minSampledEnergy",$s0,"main/Earth.cpp");         
        }
        elsif ($s0 eq "EMAX") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::Sampling::Fluency::maxSampledEnergy",$s0,"main/Earth.cpp");          
        }
        elsif ($s0 eq "NLEVELS") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::Sampling::Fluency::nSampledLevels",$s0,"main/Earth.cpp");
        }

        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }       
      }      
    }
    elsif ($s0 eq "OFF") {
       ampsConfigLib::ChangeValueOfVariable("bool Earth::Sampling::SamplingMode","false","main/Earth_Sampling.cpp");
    }     
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }  
  }
  
  
  #the maximum number of iteration used in calculating the rigidity cutoff 
  elsif ($InputLine eq "NMAXITERATIONS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::nMaxIteractions",$s0,"main/CutoffRigidity.cpp");
  }
  
    #the number, locations, energy range, and the number of the energy intervals used in the spherical sampling surfaces 
  elsif ($InputLine eq "CUTOFFTESTSPHERICALSHELLS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON")  {
      my @ShellRadiiTable;
      my $ShellRadiiTableLength=0;
      
      #read the rest of the line
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
      
        #cgeck wether the entry is the list of the shells radii
        if ($s0 eq "X") {
          while (defined $InputComment) {
            ($s0,$InputComment)=split(' ',$InputComment,2);
            
            if ( ($s0 eq "EMIN") || ($s0 eq "EMAX") || ($s0 eq "NTOTALPARTICLES") || ($s0 eq "NINJECTIONITERATIONS") ) {
              #the entry is a keyword of the section -> exist from the 'if statment' and parse the rest of the line
              last;
            }
            else {
              #the entry seems to be the next location of the samplign shell -> add it the list of the radii
              push(@ShellRadiiTable,$s0);
              $ShellRadiiTableLength++;
            }            
          }
          
          #the list of the shell's radii is complete -> add it to the sources 
          ampsConfigLib::ChangeValueOfArray("double Earth::CutoffRigidity::ShericalShells::rTestSphericalShellTable\\[\\]",\@ShellRadiiTable,"main/CutoffRigidity.cpp");
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::ShericalShells::rTestSphericalShellTableLength",$ShellRadiiTableLength,"main/CutoffRigidity.cpp");                   
        }
        
        #check whether the entry is another setting parameter
        if ($s0 eq "EMIN") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "EMAX") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "NTOTALPARTICLES") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations",$s0,"main/CutoffRigidity.cpp");
        }
        elsif ($s0 eq "NINJECTIONITERATIONS") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations",$s0,"main/CutoffRigidity.cpp");
        }    
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }       
      }      
    }
    elsif ($s0 eq "OFF") {
      #do nothing
    }     
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }  
  }
  
  #parameters of the surface mesh when calcualting the cutoff rigidity
  elsif ($InputLine eq "NZENITHELEMENTS") {
   ($s0,$InputComment)=split(' ',$InputComment,2);
    ampsConfigLib::ChangeValueOfVariable("int nZenithElements",$s0,"main/main_lib.cpp");
  }
  elsif ($InputLine eq "NAZIMUTHALELEMENTS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    ampsConfigLib::ChangeValueOfVariable("int nAzimuthalElements",$s0,"main/main_lib.cpp");
  }



  #parameters of the T96 model 
  elsif ($InputLine eq "T96") {
    ($s0,$InputComment)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);

        ampsConfigLib::ChangeValueOfVariable("bool Earth::T96::active_flag","true","main/Earth.cpp");

        if ($s0 eq "SOLAR_WIND_PRESSURE") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T96::solar_wind_pressure",$s0,"main/Earth.cpp");
        } 
        elsif ($s0 eq "DST") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T96::dst",$s0,"main/Earth.cpp");
        }
        elsif ($s0 eq "BY") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T96::by",$s0,"main/Earth.cpp");
        }
        elsif ($s0 eq "BZ") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T96::bz",$s0,"main/Earth.cpp");
        }
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }
      }
    }
  }

  
    #parameters of the T05 model 
  elsif ($InputLine eq "T05") {
    ($s0,$InputComment)=split(' ',$InputComment,2);

    if ($s0 eq "ON") {
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);

        ampsConfigLib::ChangeValueOfVariable("bool Earth::T05::active_flag","true","main/Earth.cpp");

        if ($s0 eq "SOLAR_WIND_PRESSURE") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T05::solar_wind_pressure",$s0,"main/Earth.cpp");
        } 
        elsif ($s0 eq "DST") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T05::dst",$s0,"main/Earth.cpp");
        }
        elsif ($s0 eq "BY") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T05::by",$s0,"main/Earth.cpp");
        }
        elsif ($s0 eq "BZ") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("double Earth::T05::bz",$s0,"main/Earth.cpp");
        }
   
        elsif ($s0 eq "W") {
          my @W;
        	
          ($s0,$InputComment)=split(' ',$InputComment,2); 
          push(@W,$s0);

          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@W,$s0);

          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@W,$s0);

          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@W,$s0);

          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@W,$s0);

          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@W,$s0);

          ampsConfigLib::ChangeValueOfArray("double Earth::T05::W\\[6\\]",\@W,"main/Earth.cpp");    
        }
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }
      }
    }
  }

  #define the coordinate systen for the simulation
  elsif ($InputLine eq "COORDINATESYSTEM") {
    ($s0,$InputComment)=split(' ',$InputComment,2);

    ampsConfigLib::ChangeValueOfVariable("char Exosphere::SO_FRAME\\[_MAX_STRING_LENGTH_PIC_\\]","\"".$s0."\"","main/Earth.cpp");
  }
  
  #locations of points where the cutoff rigidity and the energetic particle flux are calculated
  elsif ($InputLine eq "CUTOFFTESTLOCATIONS") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON") {
      my $nTotalLocations=0;
      my (@x0All,@x1All,@x2All,@SpecAll,@TimeAll,@SourceAll,@nPartAll);

      
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
        
        if ($s0 eq "NTOTALPARTICLES") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nTotalTestParticlesPerLocations",$s0,"main/CutoffRigidity.cpp");         
        }
        elsif ($s0 eq "NINJECTIONITERATIONS") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::IndividualLocations::nParticleInjectionIterations",$s0,"main/CutoffRigidity.cpp");         
        }
        elsif ($s0 eq "EMAX") {
          ($s0,$InputComment)=split(' ',$InputComment,2);      
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MaxEnergyLimit",$s0,"main/CutoffRigidity.cpp");   
        }
        elsif ($s0 eq "EMIN") {
          ($s0,$InputComment)=split(' ',$InputComment,2);      
          ampsConfigLib::ChangeValueOfVariable("double Earth::CutoffRigidity::IndividualLocations::MinEnergyLimit",$s0,"main/CutoffRigidity.cpp");   
        }
        elsif ($s0 eq "X") {
          my ($x0,$x1,$x2);
          
          ($x0,$x1,$x2,$InputComment)=split(' ',$InputComment,4);
          
          push(@x0All,$x0);
          push(@x1All,$x1);
          push(@x2All,$x2);    
          $nTotalLocations++;       
        }
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }
      }    
      
      #combine all entries
      if ($nTotalLocations!=0) {
        my $l0="";
        
        for (my $i=0;$i<$nTotalLocations;$i++) {
          if ($i!=0) {
            $l0=$l0.",";            
          }
          
          $l0=$l0."{".$x0All[$i].",".$x1All[$i].",".$x2All[$i]."}";
        }
        
        ampsConfigLib::ChangeValueOfVariable("int CutoffRigidityTestLocationTableLength",$nTotalLocations,"main/CutoffRigidity.cpp");
        ampsConfigLib::ChangeValueOfVariable("double CutoffRigidityTestLocationTable\\[\\]\\[3\\]","{".$l0."}","main/CutoffRigidity.cpp");    
      }
      else {
        ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::nTotalTestLocations","0","main/CutoffRigidity.cpp");
      }
       
    }
    elsif ($s0 eq "OFF") {
      #skip the test of the line
      ampsConfigLib::ChangeValueOfVariable("int Earth::CutoffRigidity::nTotalTestLocations","0","main/CutoffRigidity.cpp");
    }
    else {
      die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
    }
    
  }
  
  
  #parameters of the impulse source 
  elsif ($InputLine eq "IMPULSESOURCE") {
    ($s0,$InputComment)=split(' ',$InputComment,2);
    
    if ($s0 eq "ON") {
      #the model is turned "ON" -> Set up the model parameters 
      my $nTotalInjectionLocations=0;
      my (@x0All,@x1All,@x2All,@SpecAll,@TimeAll,@SourceAll,@nPartAll);
            
      ampsConfigLib::ChangeValueOfVariable("bool Earth::ImpulseSource::Mode","true","main/ImpulseSource.cpp"); 
            
      while (defined $InputComment) {
        ($s0,$InputComment)=split(' ',$InputComment,2);
        
        if ($s0 eq "NEW") {
          $nTotalInjectionLocations++;
        }
        elsif ($s0 eq "X") {
          my ($x0,$x1,$x2);
          
          ($x0,$x1,$x2,$InputComment)=split(' ',$InputComment,4);
          
          push(@x0All,$x0);
          push(@x1All,$x1);
          push(@x2All,$x2);          
        }
        elsif ($s0 eq "SPEC") {
          my $nspec;
          
          ($s0,$InputComment)=split(' ',$InputComment,2);
          $nspec=ampsConfigLib::GetElementNumber($s0,\@SpeciesList);
          
          push(@SpecAll,$nspec);
        }
        elsif ($s0 eq "TIME") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@TimeAll,$s0);
        }
        elsif ($s0 eq "NPART") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@nPartAll,$s0);
        }        
        elsif ($s0 eq "SOURCE") {
          ($s0,$InputComment)=split(' ',$InputComment,2);
          push(@SourceAll,$s0);
        }
        
        elsif ($s0 eq "SPECTRUM") {
          $InputComment=~s/[()]/ /;
          $InputComment=~s/[()]/ /;
          
          ($s0,$InputComment)=split(' ',$InputComment,2);
          
          if ($s0 eq "CONSTANT") {
            ($s0,$InputComment)=split(' ',$InputComment,2);
            ampsConfigLib::ChangeValueOfVariable("double Earth::ImpulseSource::EnergySpectrum::Constant::e",$s0,"main/ImpulseSource.cpp"); 
            ampsConfigLib::ChangeValueOfVariable("int Earth::ImpulseSource::EnergySpectrum::Mode","Earth::ImpulseSource::EnergySpectrum::Mode_Constatant","main/ImpulseSource.cpp");           
          }
          else {
            die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
          }
          
        }
        
        else {
          die "Cannot recognize $s0, line $InputFileLineNumber ($line) in $InputFileName.Assembled\n";
        }
      }
      
      #in case locations are defined -> set the model
      if ($nTotalInjectionLocations != 0) {
        #construct the list for the inpulse source data structure 
        my ($TotalDataStructureLine,$SingleEntry,$i);
        
        #{spec,time,false,{x0,x1,x2},Source};
        for ($i=0;$i<$nTotalInjectionLocations;$i++) {
          $SingleEntry="{".$SpecAll[$i].",".$TimeAll[$i].",false,{".$x0All[$i].",".$x1All[$i].",".$x2All[$i]."},".$SourceAll[$i].",".$nPartAll[$i]."}";
          
          if ($i==0) {
            $TotalDataStructureLine="{".$SingleEntry;
          }
          else {
            $TotalDataStructureLine=$TotalDataStructureLine.",".$SingleEntry;
          }          
        }
        
        $TotalDataStructureLine=$TotalDataStructureLine."}";
        ampsConfigLib::ChangeValueOfVariable("Earth::ImpulseSource::cImpulseSourceData Earth::ImpulseSource::ImpulseSourceData\\[\\]",$TotalDataStructureLine,"main/ImpulseSource.cpp");
        ampsConfigLib::ChangeValueOfVariable("int Earth::ImpulseSource::nTotalSourceLocations",$nTotalInjectionLocations,"main/ImpulseSource.cpp");
      }
       
    }  
  }
  
  
  
  elsif ($InputLine eq "#ENDBLOCK") {
    last;
  }   
  else {
    die "Option is unknown, line=$InputFileLineNumber ($InputFileName)\n";
  }
  
}
