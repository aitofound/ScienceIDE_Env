#!/usr/bin/perl
#$Id$
#reads the "Exosphere" section of the input file and modify the exosphere model of the PIC code

use strict;
use warnings;
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


my $InputFileName=$ARGV[0]; #"sep.input.Assembled.Block"; #$ARGV[0];  # moon.input.Assembled.Block";
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


#add the model header to the pic.h
open (PIC_H,">>$WorkingSourceDirectory/pic/pic.h") || die "Cannot open $WorkingSourceDirectory/pic/pic.h";
print PIC_H "#include \"sep.h\"\n";
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
 
  $InputLine=~s/[=()]/ /g;
  ($InputLine,$InputComment)=split(' ',$InputLine,2);
  $InputLine=~s/ //g;
  

  if ($InputLine eq "SPHEREINSIDEDOMAIN") {
    ($InputLine,$InputComment)=split(' ',$InputComment,2);
    $InputLine=~s/ //g;
     
    if ($InputLine eq "ON") {
      ampsConfigLib::ChangeValueOfVariable("static const bool SphereInsideDomain","true","main/main_lib.cpp");
    } 
    elsif ($InputLine eq "OFF") {
      ampsConfigLib::ChangeValueOfVariable("static const bool SphereInsideDomain","false","main/main_lib.cpp");
    }
    else {
      die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
    }
  }
  elsif ($InputLine eq "FORCES") {    
    $InputComment=~s/[,]/ /g;
    
    while (defined $InputComment) {
      ($InputLine,$InputComment)=split(' ',$InputComment,2);
      $InputLine=~s/ //g;
      
      
      if ($InputLine eq "OFF") {
        #do nothing
      }
      elsif ($InputLine eq "GRAVITY") {
        ampsConfigLib::RedefineMacro("_FORCE_GRAVITY_MODE_","_PIC_MODE_ON_","main/sep.h");
      }
      elsif ($InputLine eq "LORENTZ") {
        ampsConfigLib::RedefineMacro("_FORCE_LORENTZ_MODE_","_PIC_MODE_ON_","main/sep.h");
      }
      elsif ($InputLine eq "FRAMEROTATION") {
        ampsConfigLib::RedefineMacro("_FORCE_FRAMEROTATION_MODE_","_PIC_MODE_ON_","main/sep.h");
      }
      else {
        die "The option '$InputLine' is not recognized";
      }
    }
  }

 elsif($InputLine eq "STELLARANGULARVELOCITY"){
    ($InputLine,$InputComment)=split('!',$line,2);
    chomp($InputLine);
    $InputLine=~s/[=();]/ /g;
    
    ($s0,$s1,$s2)=split(' ',$InputLine,3);
    ampsConfigLib::ChangeValueOfVariable("static const double Omega",$s1,"main/sep.h");        
 }

  elsif($InputLine eq "FIELDLINEREQUESTEDLENGTH"){
    ($InputLine,$InputComment)=split(' ',$InputComment,2);
    $InputLine=~s/ //g;

    ampsConfigLib::ChangeValueOfVariable("const double FieldLineRequestedLength",$InputLine,"main/main_lib.cpp");
 }


 ##the type of the computational domain 
 elsif ($InputLine eq "DOMAINTYPE") {
   ($InputLine,$InputComment)=split(' ',$InputComment,2);
   $InputLine=~s/ //g;


   if ($InputLine eq "PARKERSPIRAL") {
     ampsConfigLib::ChangeValueOfVariable("int SEP::DomainType","SEP::DomainType_ParkerSpiral","main/mesh.cpp");
   }
   elsif ($InputLine eq "MULTIPLEPARKERSPIRALS") {
     ampsConfigLib::ChangeValueOfVariable("int SEP::DomainType","SEP::DomainType_MultipleParkerSpirals","main/mesh.cpp");
   }
   elsif ($InputLine eq "FLAMPA") {
     ampsConfigLib::ChangeValueOfVariable("int SEP::DomainType","SEP::DomainType_FLAMPA_FieldLines","main/mesh.cpp");
   }
   elsif ($InputLine eq "STRAITLINE") {
     ampsConfigLib::ChangeValueOfVariable("int SEP::DomainType","SEP::DomainType_StraitLine","main/mesh.cpp");
   }
   else {
     die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
   }
 }

  ##set the total number of Parker spirals that defines the domain
  elsif ($InputLine eq "NTOTALPARKERSPIRALS") {
    ($InputLine,$InputComment)=split(' ',$InputComment,2);

    ampsConfigLib::ChangeValueOfVariable("int SEP::Domain_nTotalParkerSpirals",$InputLine,"main/mesh.cpp");
  }

  ##set up the mover 
  elsif ($ InputLine eq "MOVER") {
    my ($f0,$f1,$f2); 

    $line=~s/[=()]/ /g;
    ($f0,$f1,$f2)=split(' ',$line,3);

    ampsConfigLib::ChangeValueOfVariable("SEP::fParticleMover SEP::ParticleMoverPtr",$f1,"main/mover.cpp");
  }

  ##the model for integrating particle trajectories 
  elsif ($InputLine eq "PARTICLETRAJECTORY") {
    ($InputLine,$InputComment)=split(' ',$InputComment,2);
    $InputLine=~s/ //g;

    if ($InputLine eq "RELATIVISTICBORIS") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleTrajectoryCalculation","SEP::ParticleTrajectoryCalculation_RelativisticBoris","main/mesh.cpp");
    }
    elsif ($InputLine eq "PARKER3D") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleTrajectoryCalculation","SEP::ParticleTrajectoryCalculation_Parker3D_MeanFreePath","main/mesh.cpp");
    }
    elsif ($InputLine eq "GUIDINGCENTER") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleTrajectoryCalculation","SEP::ParticleTrajectoryCalculation_GuidingCenter","main/mesh.cpp");
    }
    elsif ($InputLine eq "IGORFIELDLINE") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleTrajectoryCalculation","SEP::ParticleTrajectoryCalculation_IgorFieldLine","main/mesh.cpp");
    }
    elsif ($InputLine eq "FIELDLINE") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleTrajectoryCalculation","SEP::ParticleTrajectoryCalculation_FieldLine","main/mesh.cpp");
    }    else {
      die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
    }
 }


  ##type of the particle trajectory integration in 3D along a magnetic field line 
   elsif ($InputLine eq "PARTICLETRAJECTORYINTEGRATIONALONGFIELDLINE3D") {
     ($InputLine,$InputComment)=split(' ',$InputComment,2);
     $InputLine=~s/ //g;
 
     if ($InputLine eq "RK1") {
       ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleFieldLineDisplacementMethod","_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK1_","main/mover.cpp");
     }
     elsif ($InputLine eq "RK2") {
       ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleFieldLineDisplacementMethod","_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK2_","main/mover.cpp");
     }
     elsif ($InputLine eq "RK4") {
       ampsConfigLib::ChangeValueOfVariable("int SEP::ParticleFieldLineDisplacementMethod","_TRAJECTORY_INTEGRATION_FIELD_LINE_3D__RK4_","main/mover.cpp");
     }
     else {
       die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
     }
   }
 
   #set usage of the perpendicular diffusion model
   elsif ($InputLine eq "PERPENDICULARDIFFUSIONMODE") {
     ($InputLine,$InputComment)=split(' ',$InputComment,2);
     $InputLine=~s/ //g;
 
     if ($InputLine eq "ON") {
       ampsConfigLib::ChangeValueOfVariable("bool SEP::PerpendicularDiffusionMode","true","main/mover.cpp");
     }
     elsif ($InputLine eq "OFF") {
       ampsConfigLib::ChangeValueOfVariable("bool SEP::PerpendicularDiffusionMode","false","main/mover.cpp");
     }
     else {
       die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
     }
   }

   #set the "opening angle limit" for the graction of the sphere representing a spherical shock where SEPs can be injected 
   elsif ($InputLine eq "SPHERICALSHOCKOPENINGANGLELIMIT") {
    $line=~s/[=()]/ /g;
    ($s0,$s1,$s2)=split(' ',$line,3);

    ampsConfigLib::ChangeValueOfVariable("double SEP::ParticleSource::ShockWaveSphere::SphericalShockOpeningAngleLimit",$s1,"main/shock_analytical_model2D.cpp");
  } 

   ##allow particle scattering in SEP::ParticleMover_Tenishev_2005_FL 
  elsif ($InputLine eq "PARTICLEMOVER_TENISHEV_2005_FL_SCATTERING") {
    ($InputLine,$InputComment)=split(' ',$InputComment,2);
    $InputLine=~s/ //g;

    if ($InputLine eq "ON") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::Scattering::Tenishev2005AIAA::status","SEP::Scattering::Tenishev2005AIAA::_enabled","main/sep.cpp");
    }
    elsif ($InputLine eq "OFF") {
      ampsConfigLib::ChangeValueOfVariable("int SEP::Scattering::Tenishev2005AIAA::status","SEP::Scattering::Tenishev2005AIAA::_disabled","main/sep.cpp");
    }    else {
      die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
    }
 }


   ##Account for the effect of the adiabatic cooling   
  elsif ($InputLine eq "ADIABATICCOOLING") {
    ($InputLine,$InputComment)=split(' ',$InputComment,2);
    $InputLine=~s/ //g;

    if ($InputLine eq "ON") {
      ampsConfigLib::ChangeValueOfVariable("bool SEP::AccountAdiabaticCoolingFlag","true","main/mover.cpp");
    }
    elsif ($InputLine eq "OFF") {
      ampsConfigLib::ChangeValueOfVariable("bool SEP::AccountAdiabaticCoolingFlag","false","main/mover.cpp");
    }    else {
      die "The option is not recognized, line=$InputFileLineNumber ($InputFileName)\n";
    }
 }

 
 
  elsif ($InputLine eq "#ENDBLOCK") {
    last;
  }   
  else {
    die "Option is unknown, line=$InputFileLineNumber ($InputFileName)\n";
  }
  
  
  
  #extract and redefine the ProtostellarNebula's model related variables from '.ampConfig.Settings' ('.ampConfig.Settings. is created by Config.pl)
  if (-e ".ampsConfig.Settings") {
    my @Settings;
  
    open (AMPSSETTINGS,".ampsConfig.Settings") || die "Cannot open file\n";
    @Settings=<AMPSSETTINGS>;
    close (AMPSSETTINGS);
  
    
    foreach (@Settings){
#      if (/^ICESLOCATION=(.*)$/i) {ampsConfigLib::ChangeValueOfVariable("const char IcesLocationPath\\[\\]","\"".$1."\"","main/main_lib.cpp"); next};
    }    
  }

  
}



