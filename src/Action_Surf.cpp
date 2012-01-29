// Surf 
#include <cstdlib>
#include "Action_Surf.h"
#include "Constants.h" // For FOURPI
#include "CpptrajStdio.h"
#ifdef _OPENMP
#  include "omp.h"
#endif
// CONSTRUCTOR
Surf::Surf() {
  //fprintf(stderr,"Surf Con\n");
  surf=NULL;
  distances=NULL;
  soluteAtoms=0;
} 

// DESTRUCTOR
Surf::~Surf() { 
  if (distances!=NULL) free(distances);
}

// Surf::init()
/** Expected call: surf <name> <mask1> [out filename]
  */
int Surf::init() {
  char *mask1;
  char *surfFile;

  // Get keywords
  surfFile = actionArgs.getKeyString("out",NULL);

  // Get Masks
  mask1 = actionArgs.getNextMask();
  Mask1.SetMaskString(mask1);

  // Dataset to store surface area 
  surf = DSL->Add(DOUBLE, actionArgs.getNextString(),"SA");
  if (surf==NULL) return 1;
  // Add dataset to data file list
  DFL->Add(surfFile,surf);

  mprintf("    SURF: Calculating surface area for atoms in mask [%s]\n",Mask1.MaskString());

  return 0;
}

// Surf::setup()
/** Set LCPO surface area calc parameters for this parmtop if not already set. 
  * Get the mask, and check that the atoms in mask belong to solute. 
  */
int Surf::setup() {
  int matrixSize;

  if (currentParm->SetupIntegerMask( Mask1, activeReference)) return 1;
  if (Mask1.None()) {
    mprintf("    Error: Surf::setup: Mask contains 0 atoms.\n");
    return 1;
  }

  // Setup surface area calc for this parm
  soluteAtoms = currentParm->SetSurfaceInfo();
  if (soluteAtoms < 0) {
    mprinterr("Error: Surf: Could not set up surface parameters for %s.\n",currentParm->parmName);
    return 1;
  }
  mprintf("      SURF: Set up parameters for %i solute atoms.\n",soluteAtoms);
  mprintf("            LCPO surface area will be calculated for %i atoms.\n",Mask1.Nselected);

  // Check that each atom in Mask1 is part of solute
  // Create a separate mask for building the atom neighbor list
  // that only includes atoms with vdw radius > 2.5
  // Consider all atoms for icosa, only non-H's for LCPO
  atomi_neighborMask.ResetMask();
  atomi_noNeighborMask.ResetMask();
  atomj_neighborMask.ResetMask();
  for (int i=0; i < Mask1.Nselected; i++) {
    int atomi = Mask1.Selected[i];
    if (currentParm->SurfaceInfo[atomi].vdwradii > 2.5)
      atomi_neighborMask.AddAtom(atomi);
    else
      atomi_noNeighborMask.AddAtom(atomi);
    if (atomi >= soluteAtoms) {
      mprintf("Error: Surf::setup(): Atom %i in mask %s does not belong to solute.\n",
              atomi+1, Mask1.MaskString());
      return 1;
    }
  }
  // From all atoms, create a second mask for building atom neighbor list
  // that only includes atoms with vdw radius > 2.5
  for (int atomj=0; atomj < soluteAtoms; atomj++) {
    if (currentParm->SurfaceInfo[atomj].vdwradii > 2.5)
      atomj_neighborMask.AddAtom(atomj);
  }

  // Allocate distance array
  // Use half square matrix minus the diagonal
  matrixSize = ( (soluteAtoms * soluteAtoms) - soluteAtoms ) / 2;
  distances = (double*) realloc(distances, matrixSize * sizeof(double));
  return 0;  
}

// CalcIndex()
/** Given a row and column value for a half square matrix minus diagonal 
  * calculate the position in a 1D array.
  * NOTE: Is all the extra math in index calc worth it? Just have simple
  * square matrix?
  */
static int CalcIndex(int iIn, int jIn, int natom) {
  int i, j, i1;

  if (iIn > jIn) {
    j = iIn;
    i = jIn;
  } else {
    i = iIn;
    j = jIn;
  }

  i1 = i + 1;
  return ( ( (natom * i) - ((i1 * i) / 2) ) + j - i1 );
}

// Surf::CalcLCPO()
/** Return the LCPO surface area for the given atom. 
  * Adapted from gbsa=1 method in SANDER, egb.f and gbsa.h
  */
double Surf::CalcLCPO(int atomi, std::vector<int>  ineighbor) {
  double sumaij, sumajk, sumajk_2, sumaijajk;
  double tmpaij, vdw2dif, tmpajk;
  double aij, ajk;
  double vdwi, vdwi2, Si;
  double vdwj, vdwj2;
  double vdwk;
  double dij, djk;
  int distIndex;
  // Calculate LCPO surface area Ai for given atom:
  // Ai = P1*S1 + P2*Sum(Aij) + P3*Sum(Ajk) + P4*Sum(Aij * Sum(Ajk))
  sumaij = 0.0;
  sumajk = 0.0;
  sumaijajk = 0.0;

  // DEBUG - print neighbor list
  //mprintf("SURF: Neighbors for atom %i:",atomi);
  //for (std::vector<int>::iterator jt = ineighbor.begin(); jt != ineighbor.end(); jt++) {
  //  mprintf(" %i",*jt);
  //}
  //mprintf("\n");

  // Calculate surface area of atom i
  vdwi = currentParm->SurfaceInfo[atomi].vdwradii;
  vdwi2 = vdwi * vdwi;
  Si = vdwi2 * FOURPI;

  // Loop over all neighbors of atomi (j)
  // NOTE: Factor through the 2 in aij and ajk?
  for (std::vector<int>::iterator jt = ineighbor.begin(); 
                                  jt != ineighbor.end(); 
                                  jt++) 
  {
    //printf("i,j %i %i\n",atomi + 1,(*jt)+1);
    vdwj = currentParm->SurfaceInfo[*jt].vdwradii;
    vdwj2 = vdwj * vdwj;
    distIndex = CalcIndex(atomi, *jt, soluteAtoms);
    dij = distances[distIndex];
    tmpaij = vdwi - (dij * 0.5) - ( (vdwi2 - vdwj2)/(2.0 * dij) );
    aij = 2.0 * PI * vdwi * tmpaij;
    sumaij += aij;

    // Find which neighbors of atom i (j and k) are themselves neighbors
    sumajk_2 = 0.0;
    for (std::vector<int>::iterator kt = ineighbor.begin(); 
                                    kt != ineighbor.end(); 
                                    kt++) 
    {
      if ( (*kt) == (*jt) ) continue;
      //printf("i,j,k %i %i %i\n",atomi + 1,(*jt)+1,(*kt)+1);
      vdwk = currentParm->SurfaceInfo[*kt].vdwradii;
      distIndex = CalcIndex(*jt, *kt, soluteAtoms);
      djk = distances[distIndex];
      //printf("%4s%6i%6i%12.8lf\n","DJK ",(*jt)+1,(*kt)+1,djk);
      //printf("%6s%6.2lf%6.2lf\n","AVD ",vdwj,vdwk);
      if ( (vdwj + vdwk) > djk ) {
        vdw2dif = vdwj2 - (vdwk * vdwk);
        tmpajk = (2.0*vdwj) - djk - (vdw2dif / djk);
        ajk = PI*vdwj*tmpajk;
        //tmpajk = vdwj - (djk *0.5) - ( (vdwj2 - (vdwk * vdwk))/(2.0 * djk) );
        //ajk = 2.0 * PI * vdwi * tmpajk;
        //printf("%4s%6i%6i%12.8lf%12.8lf%12.8lf\n","AJK ",(*jt)+1,(*kt)+1,ajk,vdw2dif,tmpajk);
        sumajk += ajk;
        sumajk_2 += ajk;
      }
    } // END loop over neighbor-neighbor pairs of atom i (k)

    sumaijajk += (aij * sumajk_2);

    // DEBUG
    //printf("%4s%20.8lf %20.8lf %20.8lf\n","AJK ",aij,sumajk,sumaijajk);

  } // END Loop over neighbors of atom i (j)
  return( (currentParm->SurfaceInfo[atomi].P1 * Si) +
          (currentParm->SurfaceInfo[atomi].P2 * sumaij) +
          (currentParm->SurfaceInfo[atomi].P3 * sumajk) +
          (currentParm->SurfaceInfo[atomi].P4 * sumaijajk) );
}

#define SURF_USEINLINE
/* Surf::action()
 * Calculate surface area.
 */
int Surf::action() {
  double SA; 
  int atomi, atomj, distIndex;
  std::vector<int> ineighbor;

  // Step 1: Calculate distances between all pairs of atoms
  // The parallelized version uses distIndex to calculate the correct
  // index into the half-diagonal matrix. The serial version does not
  // need to do this since the distances are calculated sequentially.
#ifdef _OPENMP
#pragma omp parallel private(atomi,atomj,distIndex)
{
#pragma omp for
  for (atomi = 0; atomi < soluteAtoms - 1; atomi++) {
    for (atomj = atomi+1; atomj < soluteAtoms; atomj++) {
      distIndex = CalcIndex(atomi, atomj, soluteAtoms);
      distances[distIndex] = currentFrame->DIST(atomi, atomj);
    }
  }
}
#else
  distIndex = 0;
  for (atomi = 0; atomi < soluteAtoms - 1; atomi++) {
    for (atomj = atomi + 1; atomj < soluteAtoms; atomj++) {
      distances[distIndex++] = currentFrame->DIST(atomi, atomj);
      // DEBUG
      //mprintf("SURF_DIST:  %i %i %i %lf\n",atomi,atomj,distIndex-1,distances[distIndex-1]);
    }
  }
#endif

  // Step 2: Set up neighbor list for each atom in mask and calc its 
  // surface area contribution. Sum these up to get the total surface area.
  SA = 0.0;
#ifdef _OPENMP
#pragma omp parallel private(atomi,atomj,distIndex,ineighbor) reduction(+: SA)
{
#pragma omp for 
#endif      
  for (int maskIndex = 0; maskIndex < atomi_neighborMask.Nselected; maskIndex++) {
    atomi = atomi_neighborMask.Selected[maskIndex];
    // Vdw of atom i
    double vdwi = currentParm->SurfaceInfo[atomi].vdwradii;
    // Set up neighbor list for atom i
    ineighbor.clear();
    for (int ajidx = 0; ajidx < atomj_neighborMask.Nselected; ajidx++) {
      atomj = atomj_neighborMask.Selected[ajidx];
      if (atomi!=atomj) {
        distIndex = CalcIndex(atomi, atomj, soluteAtoms);
        // DEBUG
        //mprintf("SURF_NEIG:  %i %i %i %lf\n",atomi,atomj,distIndex,distances[distIndex]);
        // Count atoms as neighbors if their VDW radii touch
        if ( (vdwi + currentParm->SurfaceInfo[atomj].vdwradii) > distances[distIndex] ) 
          ineighbor.push_back(atomj);
      }
    }
    // Calculate surface area
    // -------------------------------------------------------------------------
#ifdef SURF_USEINLINE
    // Calculate LCPO surface area Ai for atomi:
    // Ai = P1*S1 + P2*Sum(Aij) + P3*Sum(Ajk) + P4*Sum(Aij * Sum(Ajk))
    double sumaij = 0.0;
    double sumajk = 0.0;
    double sumaijajk = 0.0;

    // DEBUG - print neighbor list
    //mprintf("SURF: Neighbors for atom %i:",atomi);
    //for (std::vector<int>::iterator jt = ineighbor.begin(); jt != ineighbor.end(); jt++) {
    //  mprintf(" %i",*jt);
    //}
    //mprintf("\n");

    // Calculate surface area of atom i
    double vdwi2 = vdwi * vdwi;
    double Si = vdwi2 * FOURPI;

    // Loop over all neighbors of atomi (j)
    // NOTE: Factor through the 2 in aij and ajk?
    for (std::vector<int>::iterator jt = ineighbor.begin(); 
                                    jt != ineighbor.end(); 
                                    jt++) 
    {
      //printf("i,j %i %i\n",atomi + 1,(*jt)+1);
      double vdwj = currentParm->SurfaceInfo[*jt].vdwradii;
      double vdwj2 = vdwj * vdwj;
      distIndex = CalcIndex(atomi, *jt, soluteAtoms);
      double dij = distances[distIndex];
      double tmpaij = vdwi - (dij * 0.5) - ( (vdwi2 - vdwj2)/(2.0 * dij) );
      double aij = 2.0 * PI * vdwi * tmpaij;
      sumaij += aij;

      // Find which neighbors of atom i (j and k) are themselves neighbors
      double sumajk_2 = 0.0;
      for (std::vector<int>::iterator kt = ineighbor.begin(); 
                                      kt != ineighbor.end(); 
                                      kt++) 
      {
        if ( (*kt) == (*jt) ) continue;
        //printf("i,j,k %i %i %i\n",atomi + 1,(*jt)+1,(*kt)+1);
        double vdwk = currentParm->SurfaceInfo[*kt].vdwradii;
        distIndex = CalcIndex(*jt, *kt, soluteAtoms);
        double djk = distances[distIndex];
        //printf("%4s%6i%6i%12.8lf\n","DJK ",(*jt)+1,(*kt)+1,djk);
        //printf("%6s%6.2lf%6.2lf\n","AVD ",vdwj,vdwk);
        if ( (vdwj + vdwk) > djk ) {
          double vdw2dif = vdwj2 - (vdwk * vdwk);
          double tmpajk = (2.0*vdwj) - djk - (vdw2dif / djk);
          double ajk = PI*vdwj*tmpajk;
          //tmpajk = vdwj - (djk *0.5) - ( (vdwj2 - (vdwk * vdwk))/(2.0 * djk) );
          //ajk = 2.0 * PI * vdwi * tmpajk;
          //printf("%4s%6i%6i%12.8lf%12.8lf%12.8lf\n","AJK ",(*jt)+1,(*kt)+1,ajk,vdw2dif,tmpajk);
          sumajk += ajk;
          sumajk_2 += ajk;
        }
      } // END loop over neighbor-neighbor pairs of atom i (kt)

      sumaijajk += (aij * sumajk_2);

      // DEBUG
      //printf("%4s%20.8lf %20.8lf %20.8lf\n","AJK ",aij,sumajk,sumaijajk);

    } // END Loop over neighbors of atom i (jt)
    SA += ( (currentParm->SurfaceInfo[atomi].P1 * Si       ) +
            (currentParm->SurfaceInfo[atomi].P2 * sumaij   ) +
            (currentParm->SurfaceInfo[atomi].P3 * sumajk   ) +
            (currentParm->SurfaceInfo[atomi].P4 * sumaijajk) 
          );
#else
    SA+=CalcLCPO(atomi,ineighbor);
#endif
    // -------------------------------------------------------------------------
  } // END Loop over atoms in mask (atomi) 
#ifdef _OPENMP
} // END pragma omp parallel
#endif
  // Second loop over atoms with no neighbors (vdw <= 2.5)
  for (int maskIndex=0; maskIndex < atomi_noNeighborMask.Nselected; maskIndex++) {
    int atomi = atomi_noNeighborMask.Selected[maskIndex];
    // Vdw of atom i
    double vdwi = currentParm->SurfaceInfo[atomi].vdwradii;
    // Calculate surface area of atom i
    double vdwi2 = vdwi * vdwi;
    double Si = vdwi2 * FOURPI; 
    SA += (currentParm->SurfaceInfo[atomi].P1 * Si);
  }

  surf->Add(frameNum, &SA);

  return 0;
} 


