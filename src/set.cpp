/* ----------------------------------------------------------------------
   LIGGGHTS - LAMMPS Improved for General Granular and Granular Heat
   Transfer Simulations

   LIGGGHTS is part of the CFDEMproject
   www.liggghts.com | www.cfdem.com

   This file was modified with respect to the release in LAMMPS
   Modifications are Copyright 2009-2012 JKU Linz
                     Copyright 2012-     DCS Computing GmbH, Linz

   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level directory.
------------------------------------------------------------------------- */

#include "mpi.h"
#include "math.h"
#include "stdlib.h"
#include "string.h"
#include "set.h"
#include "atom.h"
#include "atom_vec.h"
#include "atom_vec_ellipsoid.h"
#include "atom_vec_line.h"
#include "atom_vec_tri.h"
#include "domain.h"
#include "region.h"
#include "group.h"
#include "comm.h"
#include "neighbor.h"
#include "force.h"
#include "pair.h"
#include "random_park.h"
#include "math_extra.h"
#include "math_const.h"
#include "error.h"
#include "modify.h" 
#include "fix_property_atom.h" 
#include "sph_kernels.h" 
#include "fix_sph.h" 

using namespace LAMMPS_NS;
using namespace MathConst;

enum{ATOM_SELECT,MOL_SELECT,TYPE_SELECT,GROUP_SELECT,REGION_SELECT};
enum{TYPE,TYPE_FRACTION,MOLECULE,X,Y,Z,CHARGE,MASS,SHAPE,LENGTH,TRI,
     DIPOLE,DIPOLE_RANDOM,QUAT,QUAT_RANDOM,THETA,ANGMOM,
     DIAMETER,DENSITY,VOLUME,IMAGE,BOND,ANGLE,DIHEDRAL,IMPROPER,
     MESO_E,MESO_CV,MESO_RHO,
     VX,VY,VZ,OMEGAX,OMEGAY,OMEGAZ,PROPERTYPERATOM}; 

#define BIG INT_MAX

/* ---------------------------------------------------------------------- */

void Set::command(int narg, char **arg)
{
  if (domain->box_exist == 0)
    error->all(FLERR,"Set command before simulation box is defined");
  if (atom->natoms == 0)
    error->all(FLERR,"Set command with no atoms existing");
  if (narg < 3) error->all(FLERR,"Illegal set command");

  // style and ID info

  if (strcmp(arg[0],"atom") == 0) style = ATOM_SELECT;
  else if (strcmp(arg[0],"mol") == 0) style = MOL_SELECT;
  else if (strcmp(arg[0],"type") == 0) style = TYPE_SELECT;
  else if (strcmp(arg[0],"group") == 0) style = GROUP_SELECT;
  else if (strcmp(arg[0],"region") == 0) style = REGION_SELECT;
  else error->all(FLERR,"Illegal set command");

  int n = strlen(arg[1]) + 1;
  id = new char[n];
  strcpy(id,arg[1]);
  select = NULL;

  // loop over keyword/value pairs
  // call appropriate routine to reset attributes

  if (comm->me == 0 && screen) fprintf(screen,"Setting atom values ...\n");

  int allcount,origarg;

  int iarg = 2;
  while (iarg < narg) {
    count = 0;
    origarg = iarg;

    if (strcmp(arg[iarg],"type") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (ivalue <= 0 || ivalue > atom->ntypes)
        error->all(FLERR,"Invalid value in set command");
      set(TYPE);
      iarg += 2;
    } else if (strcmp(arg[iarg],"type/fraction") == 0) {
      if (iarg+4 > narg) error->all(FLERR,"Illegal set command");
      newtype = atoi(arg[iarg+1]);
      fraction = atof(arg[iarg+2]);
      ivalue = atoi(arg[iarg+3]);
      if (newtype <= 0 || newtype > atom->ntypes)
        error->all(FLERR,"Invalid value in set command");
      if (fraction < 0.0 || fraction > 1.0)
        error->all(FLERR,"Invalid value in set command");
      if (ivalue <= 0)
        error->all(FLERR,"Invalid random number seed in set command");
      setrandom(TYPE_FRACTION);
      iarg += 4;
    } else if (strcmp(arg[iarg],"mol") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (!atom->molecule_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(MOLECULE);
      iarg += 2;
    } else if (strcmp(arg[iarg],"x") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(X);
      iarg += 2;
    } else if (strcmp(arg[iarg],"y") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(Y);
      iarg += 2;
    } else if (strcmp(arg[iarg],"z") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(Z);
      iarg += 2;
    } else if (strcmp(arg[iarg],"vx") == 0) {  
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(VX);
      iarg += 2;
    } else if (strcmp(arg[iarg],"vy") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(VY);
      iarg += 2;
    } else if (strcmp(arg[iarg],"vz") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(VZ);
      iarg += 2;
    } else if (strcmp(arg[iarg],"omegax") == 0) { 
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(OMEGAX);
      iarg += 2;
    } else if (strcmp(arg[iarg],"omegay") == 0) {  
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(OMEGAY);
      iarg += 2;
    } else if (strcmp(arg[iarg],"omegaz") == 0) {  
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      set(OMEGAZ);
      iarg += 2;
    } else if (strcmp(arg[iarg],"charge") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->q_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(CHARGE);
      iarg += 2;
    } else if (strcmp(arg[iarg],"mass") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->rmass_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (dvalue <= 0.0) error->all(FLERR,"Invalid mass in set command");
      set(MASS);
      iarg += 2;
    } else if (strcmp(arg[iarg],"shape") == 0) {
      if (iarg+4 > narg) error->all(FLERR,"Illegal set command");
      xvalue = atof(arg[iarg+1]);
      yvalue = atof(arg[iarg+2]);
      zvalue = atof(arg[iarg+3]);
      if (!atom->ellipsoid_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (xvalue < 0.0 || yvalue < 0.0 || zvalue < 0.0)
        error->all(FLERR,"Invalid shape in set command");
      if (xvalue > 0.0 || yvalue > 0.0 || zvalue > 0.0) {
        if (xvalue == 0.0 || yvalue == 0.0 || zvalue == 0.0)
          error->one(FLERR,"Invalid shape in set command");
      }
      set(SHAPE);
      iarg += 4;
    } else if (strcmp(arg[iarg],"length") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->line_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (dvalue < 0.0) error->all(FLERR,"Invalid length in set command");
      set(LENGTH);
      iarg += 2;
    } else if (strcmp(arg[iarg],"tri") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->tri_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (dvalue < 0.0) error->all(FLERR,"Invalid length in set command");
      set(TRI);
      iarg += 2;
    } else if (strcmp(arg[iarg],"dipole") == 0) {
      if (iarg+4 > narg) error->all(FLERR,"Illegal set command");
      xvalue = atof(arg[iarg+1]);
      yvalue = atof(arg[iarg+2]);
      zvalue = atof(arg[iarg+3]);
      if (!atom->mu_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(DIPOLE);
      iarg += 4;
    } else if (strcmp(arg[iarg],"dipole/random") == 0) {
      if (iarg+3 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      dvalue = atof(arg[iarg+2]);
      if (!atom->mu_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0)
        error->all(FLERR,"Invalid random number seed in set command");
      if (dvalue <= 0.0)
        error->all(FLERR,"Invalid dipole length in set command");
      setrandom(DIPOLE_RANDOM);
      iarg += 3;
    } else if (strcmp(arg[iarg],"quat") == 0) {
      if (iarg+5 > narg) error->all(FLERR,"Illegal set command");
      xvalue = atof(arg[iarg+1]);
      yvalue = atof(arg[iarg+2]);
      zvalue = atof(arg[iarg+3]);
      wvalue = atof(arg[iarg+4]);
      if (!atom->ellipsoid_flag && !atom->tri_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(QUAT);
      iarg += 5;
    } else if (strcmp(arg[iarg],"quat/random") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (!atom->ellipsoid_flag && !atom->tri_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0)
        error->all(FLERR,"Invalid random number seed in set command");
      setrandom(QUAT_RANDOM);
      iarg += 2;
    } else if (strcmp(arg[iarg],"theta") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      dvalue *= MY_PI/180.0;
      if (!atom->line_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(THETA);
      iarg += 2;
    } else if (strcmp(arg[iarg],"angmom") == 0) {
      if (iarg+4 > narg) error->all(FLERR,"Illegal set command");
      xvalue = atof(arg[iarg+1]);
      yvalue = atof(arg[iarg+2]);
      zvalue = atof(arg[iarg+3]);
      if (!atom->ellipsoid_flag && !atom->tri_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(ANGMOM);
      iarg += 4;
    } else if (strcmp(arg[iarg],"diameter") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1])*force->cg();
      if (!atom->radius_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (dvalue < 0.0) error->all(FLERR,"Invalid diameter in set command");
      set(DIAMETER);
      iarg += 2;
    } else if (strcmp(arg[iarg],"density") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->rmass_flag && !atom->density_flag) 
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(DENSITY);
      iarg += 2;
    } else if (strcmp(arg[iarg],"volume") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->vfrac_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(VOLUME);
      iarg += 2;
    } else if (strcmp(arg[iarg],"image") == 0) {
      if (iarg+4 > narg) error->all(FLERR,"Illegal set command");
      ximageflag = yimageflag = zimageflag = 0;
      if (strcmp(arg[iarg+1],"NULL") != 0) {
        ximageflag = 1;
        ximage = atoi(arg[iarg+1]);
      }
      if (strcmp(arg[iarg+2],"NULL") != 0) {
        yimageflag = 1;
        yimage = atoi(arg[iarg+2]);
      }
      if (strcmp(arg[iarg+3],"NULL") != 0) {
        zimageflag = 1;
        zimage = atoi(arg[iarg+3]);
      }
      if (ximageflag && ximage && !domain->xperiodic)
        error->all(FLERR,
                   "Cannot set non-zero image flag for non-periodic dimension");
      if (yimageflag && yimage && !domain->yperiodic)
        error->all(FLERR,
                   "Cannot set non-zero image flag for non-periodic dimension");
      if (zimageflag && zimage && !domain->zperiodic)
        error->all(FLERR,
                   "Cannot set non-zero image flag for non-periodic dimension");
      set(IMAGE);
      iarg += 4;
    } else if (strcmp(arg[iarg],"bond") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (atom->avec->bonds_allow == 0)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0 || ivalue > atom->nbondtypes)
        error->all(FLERR,"Invalid value in set command");
      topology(BOND);
      iarg += 2;
    } else if (strcmp(arg[iarg],"angle") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (atom->avec->angles_allow == 0)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0 || ivalue > atom->nangletypes)
        error->all(FLERR,"Invalid value in set command");
      topology(ANGLE);
      iarg += 2;
    } else if (strcmp(arg[iarg],"dihedral") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (atom->avec->dihedrals_allow == 0)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0 || ivalue > atom->ndihedraltypes)
        error->all(FLERR,"Invalid value in set command");
      topology(DIHEDRAL);
      iarg += 2;
    } else if (strcmp(arg[iarg],"improper") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      ivalue = atoi(arg[iarg+1]);
      if (atom->avec->impropers_allow == 0)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      if (ivalue <= 0 || ivalue > atom->nimpropertypes)
        error->all(FLERR,"Invalid value in set command");
      topology(IMPROPER);
      iarg += 2;
    } else if (strcmp(arg[iarg],"meso_e") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->e_flag)
        error->all(FLERR,"Cannot set this attribute for this atom style");
      set(MESO_E);
      iarg += 2;
    } else if (strcmp(arg[iarg],"meso_cv") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->cv_flag)
            error->all(FLERR,"Cannot set this attribute for this atom style");
      set(MESO_CV);
      iarg += 2;
    } else if (strcmp(arg[iarg],"meso_rho") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal set command");
      dvalue = atof(arg[iarg+1]);
      if (!atom->rho_flag)
        error->all(FLERR,"Cannot set meso_rho for this atom style");
      set(MESO_RHO);
      iarg += 2;
    } else if (strncmp(arg[iarg],"property/atom",13) == 0) { 
      if (iarg+1 > narg)
        error->all(FLERR,"Illegal set command for property/atom");
      int n = strlen(arg[iarg+1]) + 1;
      char* variablename = new char[n];
      strcpy(variablename,arg[iarg+1]);
      //find the fix (there should be only one fix with the same variablename, this is ensured by the fix itself)
      updFix = NULL;
      for (int ifix = 0; ifix < (lmp->modify->nfix); ifix++){
        if ((strncmp(modify->fix[ifix]->style,"property/atom",13) == 0) && (strcmp(((FixPropertyAtom*)(modify->fix[ifix]))->variablename,variablename)==0) ){
            updFix=(FixPropertyAtom*)(lmp->modify->fix[ifix]);
        }
      }
      delete []variablename;
      if (updFix==NULL)
        error->all(FLERR,"Could not identify the per-atom property you want to set");

      nUpdValues=updFix->nvalues;
      if (nUpdValues != (narg-iarg-2) )
        error->all(FLERR,"The number of values for the set property/atom does not match the number needed");
      updValues = new double[nUpdValues];
      for(int j=0;j<nUpdValues ;j++)
        updValues[j]=atof(arg[iarg+1+1+j]);
      set(PROPERTYPERATOM);
      delete []updValues;
      iarg += (2+nUpdValues);
    } else if (strcmp(arg[iarg],"sphkernel") == 0) { 
      if (iarg+2 > narg) error->all(FLERR, "Illegal set command");

      // check uniqueness of kernel IDs

      int flag = SPH_KERNEL_NS::sph_kernels_unique_id();
      if(flag < 0) error->all(FLERR, "Cannot proceed, sph kernels need unique IDs, check all sph_kernel_* files");

      // get kernel id

      dvalue = SPH_KERNEL_NS::sph_kernel_id(arg[iarg+1]);
      if(dvalue < 0) error->all(FLERR, "Illegal pair_style sph command, unknown sph kernel");

      // set kernel_id in all sph fixes

      if (comm->me == 0 && screen) {
        fprintf(screen,"Setting undefined fix_sph kernel IDs ...\n");
        fprintf(screen,"  Sph styles with undefined kernel_id found: \n");
      }
      for (int ifix = 0; ifix < modify->nfix; ifix++)
      {
        if (strstr(modify->fix[ifix]->style,"sph")) {
          if (((FixSph *)(modify->fix[ifix]))->kernel_flag && ((FixSph *)(modify->fix[ifix]))->get_kernel_id() < 0) {
            if (comm->me == 0 && screen) fprintf(screen,"  Fix style = %s\n",modify->fix[ifix]->style);
            ((FixSph *)(modify->fix[ifix]))->set_kernel_id(dvalue);
            count++;
          }
        }
      }

      iarg += 2;
    } else error->all(FLERR,"Illegal set command");

    // statistics

    MPI_Allreduce(&count,&allcount,1,MPI_INT,MPI_SUM,world);

    if (comm->me == 0) {
      if (screen) fprintf(screen,"  %d settings made for %s\n",
                          allcount,arg[origarg]);
      if (logfile) fprintf(logfile,"  %d settings made for %s\n",
                           allcount,arg[origarg]);
    }
  }

  // free local memory

  delete [] id;
  delete [] select;
}

/* ----------------------------------------------------------------------
   select atoms according to ATOM, MOLECULE, TYPE, GROUP, REGION style
   n = nlocal or nlocal+nghost depending on keyword
------------------------------------------------------------------------- */

void Set::selection(int n)
{
  delete [] select;
  select = new int[n];
  int nlo,nhi;

  if (style == ATOM_SELECT) {
    if (atom->tag_enable == 0)
      error->all(FLERR,"Cannot use set atom with no atom IDs defined");
    force->bounds(id,BIG,nlo,nhi);

    int *tag = atom->tag;
    for (int i = 0; i < n; i++)
      if (tag[i] >= nlo && tag[i] <= nhi) select[i] = 1;
      else select[i] = 0;

  } else if (style == MOL_SELECT) {
    if (atom->molecule_flag == 0)
      error->all(FLERR,"Cannot use set mol with no molecule IDs defined");
    if (strcmp(id,"0") == 0) nlo = nhi = 0;
    else force->bounds(id,BIG,nlo,nhi);

    int *molecule = atom->molecule;
    for (int i = 0; i < n; i++)
      if (molecule[i] >= nlo && molecule[i] <= nhi) select[i] = 1;
      else select[i] = 0;

  } else if (style == TYPE_SELECT) {
    force->bounds(id,atom->ntypes,nlo,nhi);

    int *type = atom->type;
    for (int i = 0; i < n; i++)
      if (type[i] >= nlo && type[i] <= nhi) select[i] = 1;
      else select[i] = 0;

  } else if (style == GROUP_SELECT) {
    int igroup = group->find(id);
    if (igroup == -1) error->all(FLERR,"Could not find set group ID");
    int groupbit = group->bitmask[igroup];

    int *mask = atom->mask;
    for (int i = 0; i < n; i++)
      if (mask[i] & groupbit) select[i] = 1;
      else select[i] = 0;

  } else if (style == REGION_SELECT) {
    int iregion = domain->find_region(id);
    if (iregion == -1) error->all(FLERR,"Set region ID does not exist");

    double **x = atom->x;
    for (int i = 0; i < n; i++)
      if (domain->regions[iregion]->match(x[i][0],x[i][1],x[i][2]))
        select[i] = 1;
      else select[i] = 0;
  }
}

/* ----------------------------------------------------------------------
   set an owned atom property directly
------------------------------------------------------------------------- */

void Set::set(int keyword)
{
  AtomVecEllipsoid *avec_ellipsoid =
    (AtomVecEllipsoid *) atom->style_match("ellipsoid");
  AtomVecLine *avec_line = (AtomVecLine *) atom->style_match("line");
  AtomVecTri *avec_tri = (AtomVecTri *) atom->style_match("tri");

  selection(atom->nlocal);

  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    if (!select[i]) continue;

    if (keyword == TYPE) atom->type[i] = ivalue;
    else if (keyword == MOLECULE) atom->molecule[i] = ivalue;
    else if (keyword == X) atom->x[i][0] = dvalue;
    else if (keyword == Y) atom->x[i][1] = dvalue;
    else if (keyword == Z) atom->x[i][2] = dvalue;
      else if (keyword == VX) atom->v[i][0] = dvalue; 
      else if (keyword == VY) atom->v[i][1] = dvalue;
      else if (keyword == VZ) atom->v[i][2] = dvalue;
      else if (keyword == OMEGAX) atom->omega[i][0] = dvalue;  
      else if (keyword == OMEGAY) atom->omega[i][1] = dvalue;  
      else if (keyword == OMEGAZ) atom->omega[i][2] = dvalue;  
    else if (keyword == CHARGE) atom->q[i] = dvalue;
    else if (keyword == MASS) atom->rmass[i] = dvalue;
    else if (keyword == DIAMETER) 
    {
        atom->radius[i] = 0.5 * dvalue;
        if(atom->rmass_flag && atom->density_flag && atom->density[i] > 0.)
        {
          if (domain->dimension == 2)
            atom->rmass[i] = MY_PI * atom->radius[i]*atom->radius[i] * atom->density[i];
          else
            atom->rmass[i] = 4.0*MY_PI/3.0 * atom->radius[i]*atom->radius[i]*atom->radius[i] * atom->density[i];
        }
    }
    else if (keyword == VOLUME) atom->vfrac[i] = dvalue;
    else if (keyword == MESO_E) atom->e[i] = dvalue;
    else if (keyword == MESO_CV) atom->cv[i] = dvalue;
    else if (keyword == MESO_RHO) atom->rho[i] = dvalue;

    // set desired per-atom property
    else if (keyword == PROPERTYPERATOM) {

        // if fix was just created, its default values have not been set,
        // so have to add a run 0 to call setup
        if(updFix->just_created)
            error->all(FLERR,"May not use the set command right after fix property/atom without a prior run. Add a 'run 0' between fix property/atom and set");

            if (updFix->data_style) for (int m = 0; m < nUpdValues; m++)
              updFix->array_atom[i][m] = updValues[m];
        else updFix->vector_atom[i]=updValues[0];

    }

    // set shape of ellipsoidal particle

    else if (keyword == SHAPE)
      avec_ellipsoid->set_shape(i,0.5*xvalue,0.5*yvalue,0.5*zvalue);

    // set length of line particle

    else if (keyword == LENGTH)
      avec_line->set_length(i,dvalue);

    // set corners of tri particle

    else if (keyword == TRI)
      avec_tri->set_equilateral(i,dvalue);

    // set rmass via density
    // if radius > 0.0, treat as sphere
    // if shape > 0.0, treat as ellipsoid
    // if length > 0.0, treat as line
    // if area > 0.0, treat as tri
    // else set rmass to density directly

    else if (keyword == DENSITY) {
      if (atom->radius_flag && atom->radius[i] > 0.0)
      {
          atom->density[i] = dvalue;
          if (domain->dimension == 2)
            atom->rmass[i] = MY_PI * atom->radius[i]*atom->radius[i] * atom->density[i]; 
          else
            atom->rmass[i] = 4.0*MY_PI/3.0 * atom->radius[i]*atom->radius[i]*atom->radius[i] * atom->density[i]; 
      }
      else if (atom->density_flag)
        atom->density[i] = dvalue;
      else if (atom->ellipsoid_flag && atom->ellipsoid[i] >= 0) {
        double *shape = avec_ellipsoid->bonus[atom->ellipsoid[i]].shape;
        atom->rmass[i] = 4.0*MY_PI/3.0 * shape[0]*shape[1]*shape[2] * dvalue;
      } else if (atom->line_flag && atom->line[i] >= 0) {
        double length = avec_line->bonus[atom->line[i]].length;
        atom->rmass[i] = length * dvalue;
      } else if (atom->tri_flag && atom->tri[i] >= 0) {
        double *c1 = avec_tri->bonus[atom->tri[i]].c1;
        double *c2 = avec_tri->bonus[atom->tri[i]].c2;
        double *c3 = avec_tri->bonus[atom->tri[i]].c3;
        double c2mc1[2],c3mc1[3];
        MathExtra::sub3(c2,c1,c2mc1);
        MathExtra::sub3(c3,c1,c3mc1);
        double norm[3];
        MathExtra::cross3(c2mc1,c3mc1,norm);
        double area = 0.5 * MathExtra::len3(norm);
        atom->rmass[i] = area * dvalue;
      } else atom->rmass[i] = dvalue;

    // reset any or all of 3 image flags

    } else if (keyword == IMAGE) {
      int xbox = (atom->image[i] & 1023) - 512;
      int ybox = (atom->image[i] >> 10 & 1023) - 512;
      int zbox = (atom->image[i] >> 20) - 512;
      if (ximageflag) xbox = ximage;
      if (yimageflag) ybox = yimage;
      if (zimageflag) zbox = zimage;
      atom->image[i] = ((zbox + 512 & 1023) << 20) |
        ((ybox + 512 & 1023) << 10) | (xbox + 512 & 1023);

    // set dipole moment

    } else if (keyword == DIPOLE) {
      double **mu = atom->mu;
      mu[i][0] = xvalue;
      mu[i][1] = yvalue;
      mu[i][2] = zvalue;
      mu[i][3] = sqrt(mu[i][0]*mu[i][0] + mu[i][1]*mu[i][1] +
                      mu[i][2]*mu[i][2]);

    // set quaternion orientation of ellipsoid or tri particle

    } else if (keyword == QUAT) {
      double *quat;
      if (avec_ellipsoid && atom->ellipsoid[i] >= 0)
        quat = avec_ellipsoid->bonus[atom->ellipsoid[i]].quat;
      else if (avec_tri && atom->tri[i] >= 0)
        quat = avec_tri->bonus[atom->tri[i]].quat;
      else
        error->one(FLERR,"Cannot set quaternion for atom that has none");

      double theta2 = MY_PI2 * wvalue/180.0;
      double sintheta2 = sin(theta2);
      quat[0] = cos(theta2);
      quat[1] = xvalue * sintheta2;
      quat[2] = yvalue * sintheta2;
      quat[3] = zvalue * sintheta2;
      MathExtra::qnormalize(quat);

    // set theta of line particle

    } else if (keyword == THETA) {
      if (atom->line[i] < 0)
        error->one(FLERR,"Cannot set theta for atom that is not a line");
      avec_line->bonus[atom->line[i]].theta = dvalue;

    // set angmom of ellipsoidal or tri particle

    } else if (keyword == ANGMOM) {
      atom->angmom[i][0] = xvalue;
      atom->angmom[i][1] = yvalue;
      atom->angmom[i][2] = zvalue;
    }

    count++;
  }
}

/* ----------------------------------------------------------------------
   set an owned atom property randomly
   set seed based on atom tag
   make atom result independent of what proc owns it
------------------------------------------------------------------------- */

void Set::setrandom(int keyword)
{
  int i;

  AtomVecEllipsoid *avec_ellipsoid =
    (AtomVecEllipsoid *) atom->style_match("ellipsoid");
  AtomVecLine *avec_line = (AtomVecLine *) atom->style_match("line");
  AtomVecTri *avec_tri = (AtomVecTri *) atom->style_match("tri");

  selection(atom->nlocal);
  RanPark *random = new RanPark(lmp,1);
  double **x = atom->x;
  int seed = ivalue;

  // set fraction of atom types to newtype

  if (keyword == TYPE_FRACTION) {
    int nlocal = atom->nlocal;

    for (i = 0; i < nlocal; i++)
      if (select[i]) {
        random->reset(seed,x[i]);
        if (random->uniform() > fraction) continue;
        atom->type[i] = newtype;
        count++;
      }

  // set dipole moments to random orientations in 3d or 2d
  // dipole length is determined by dipole type array

  } else if (keyword == DIPOLE_RANDOM) {
    double **mu = atom->mu;
    int nlocal = atom->nlocal;

    double msq,scale;

    if (domain->dimension == 3) {
      for (i = 0; i < nlocal; i++)
        if (select[i]) {
          random->reset(seed,x[i]);
          mu[i][0] = random->uniform() - 0.5;
          mu[i][1] = random->uniform() - 0.5;
          mu[i][2] = random->uniform() - 0.5;
          msq = mu[i][0]*mu[i][0] + mu[i][1]*mu[i][1] + mu[i][2]*mu[i][2];
          scale = dvalue/sqrt(msq);
          mu[i][0] *= scale;
          mu[i][1] *= scale;
          mu[i][2] *= scale;
          mu[i][3] = dvalue;
          count++;
        }

    } else {
      for (i = 0; i < nlocal; i++)
        if (select[i]) {
          random->reset(seed,x[i]);
          mu[i][0] = random->uniform() - 0.5;
          mu[i][1] = random->uniform() - 0.5;
          mu[i][2] = 0.0;
          msq = mu[i][0]*mu[i][0] + mu[i][1]*mu[i][1];
          scale = dvalue/sqrt(msq);
          mu[i][0] *= scale;
          mu[i][1] *= scale;
          mu[i][3] = dvalue;
          count++;
        }
    }

  // set quaternions to random orientations in 3d or 2d

  } else if (keyword == QUAT_RANDOM) {
    int *ellipsoid = atom->ellipsoid;
    int *tri = atom->tri;
    int nlocal = atom->nlocal;
    double *quat;

    if (domain->dimension == 3) {
      double s,t1,t2,theta1,theta2;
      for (i = 0; i < nlocal; i++)
        if (select[i]) {
          if (avec_ellipsoid && atom->ellipsoid[i] >= 0)
            quat = avec_ellipsoid->bonus[atom->ellipsoid[i]].quat;
          else if (avec_tri && atom->tri[i] >= 0)
            quat = avec_tri->bonus[atom->tri[i]].quat;
          else
            error->one(FLERR,"Cannot set quaternion for atom that has none");

          random->reset(seed,x[i]);
          s = random->uniform();
          t1 = sqrt(1.0-s);
          t2 = sqrt(s);
          theta1 = 2.0*MY_PI*random->uniform();
          theta2 = 2.0*MY_PI*random->uniform();
          quat[0] = cos(theta2)*t2;
          quat[1] = sin(theta1)*t1;
          quat[2] = cos(theta1)*t1;
          quat[3] = sin(theta2)*t2;
          count++;
        }

    } else {
      double theta2;
      for (i = 0; i < nlocal; i++)
        if (select[i]) {
          if (avec_ellipsoid && atom->ellipsoid[i] >= 0)
            quat = avec_ellipsoid->bonus[atom->ellipsoid[i]].quat;
          else
            error->one(FLERR,"Cannot set quaternion for atom that has none");

          random->reset(seed,x[i]);
          theta2 = MY_PI*random->uniform();
          quat[0] = cos(theta2);
          quat[1] = 0.0;
          quat[2] = 0.0;
          quat[3] = sin(theta2);
          count++;
        }
    }
  }

  delete random;
}

/* ---------------------------------------------------------------------- */

void Set::topology(int keyword)
{
  int m,atom1,atom2,atom3,atom4;

  // border swap to acquire ghost atom info
  // enforce PBC before in case atoms are outside box
  // init entire system since comm->exchange is done
  // comm::init needs neighbor::init needs pair::init needs kspace::init, etc

  if (comm->me == 0 && screen) fprintf(screen,"  system init for set ...\n");
  lmp->init();

  if (domain->triclinic) domain->x2lamda(atom->nlocal);
  domain->pbc();
  domain->reset_box();
  comm->setup();
  comm->exchange();
  comm->borders();
  if (domain->triclinic) domain->lamda2x(atom->nlocal+atom->nghost);

  // select both owned and ghost atoms

  selection(atom->nlocal + atom->nghost);

  // for BOND, each of 2 atoms must be in group

  if (keyword == BOND) {
    int nlocal = atom->nlocal;
    for (int i = 0; i < nlocal; i++)
      for (m = 0; m < atom->num_bond[i]; m++) {
        atom1 = atom->map(atom->bond_atom[i][m]);
        if (atom1 == -1) error->one(FLERR,"Bond atom missing in set command");
        if (select[i] && select[atom1]) {
          atom->bond_type[i][m] = ivalue;
          count++;
        }
      }
  }

  // for ANGLE, each of 3 atoms must be in group

  if (keyword == ANGLE) {
    int nlocal = atom->nlocal;
    for (int i = 0; i < nlocal; i++)
      for (m = 0; m < atom->num_angle[i]; m++) {
        atom1 = atom->map(atom->angle_atom1[i][m]);
        atom2 = atom->map(atom->angle_atom2[i][m]);
        atom3 = atom->map(atom->angle_atom3[i][m]);
        if (atom1 == -1 || atom2 == -1 || atom3 == -1)
          error->one(FLERR,"Angle atom missing in set command");
        if (select[atom1] && select[atom2] && select[atom3]) {
          atom->angle_type[i][m] = ivalue;
          count++;
        }
      }
  }

  // for DIHEDRAL, each of 4 atoms must be in group

  if (keyword == DIHEDRAL) {
    int nlocal = atom->nlocal;
    for (int i = 0; i < nlocal; i++)
      for (m = 0; m < atom->num_dihedral[i]; m++) {
        atom1 = atom->map(atom->dihedral_atom1[i][m]);
        atom2 = atom->map(atom->dihedral_atom2[i][m]);
        atom3 = atom->map(atom->dihedral_atom3[i][m]);
        atom4 = atom->map(atom->dihedral_atom4[i][m]);
        if (atom1 == -1 || atom2 == -1 || atom3 == -1 || atom4 == -1)
          error->one(FLERR,"Dihedral atom missing in set command");
        if (select[atom1] && select[atom2] && select[atom3] && select[atom4]) {
          atom->dihedral_type[i][m] = ivalue;
          count++;
        }
      }
  }

  // for IMPROPER, each of 4 atoms must be in group

  if (keyword == IMPROPER) {
    int nlocal = atom->nlocal;
    for (int i = 0; i < nlocal; i++)
      for (m = 0; m < atom->num_improper[i]; m++) {
        atom1 = atom->map(atom->improper_atom1[i][m]);
        atom2 = atom->map(atom->improper_atom2[i][m]);
        atom3 = atom->map(atom->improper_atom3[i][m]);
        atom4 = atom->map(atom->improper_atom4[i][m]);
        if (atom1 == -1 || atom2 == -1 || atom3 == -1 || atom4 == -1)
          error->one(FLERR,"Improper atom missing in set command");
        if (select[atom1] && select[atom2] && select[atom3] && select[atom4]) {
          atom->improper_type[i][m] = ivalue;
          count++;
        }
      }
  }
}
