
#include "pic.h"

#include "Exosphere.h"

double Exosphere::OrbitalMotion::GetTAA(SpiceDouble et) {return 0.0;}
int Exosphere::ColumnIntegral::GetVariableList(char *vlist) {return 0;}
void Exosphere::ColumnIntegral::ProcessColumnIntegrationVector(double *res,int resLength) {}
double Exosphere::GetSurfaceTemperature(double cosSubsolarAngle,double *x_LOCAL_SO_OBJECT) {return 0.0;}
char Exosphere::SO_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::IAU_FRAME[_MAX_STRING_LENGTH_PIC_]="GALL_EPHIOD";
char Exosphere::ObjectName[_MAX_STRING_LENGTH_PIC_]="Europa";
void Exosphere::ColumnIntegral::CoulumnDensityIntegrant(double *res,int resLength,double* x,cTreeNodeAMR<PIC::Mesh::cDataBlockAMR>* node) {}
double Exosphere::SurfaceInteraction::StickingProbability(int spec,double& ReemissionParticleFraction,double Temp) {return 0.0;}
