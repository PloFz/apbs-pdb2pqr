/**
 *  @file    main.c
 *  @author  Nathan Baker
 *  @brief   APBS "front end" program using formatted input files.
 * 
 *           This driver program represents a mish-mash of
 *           instructions for
 *           calculating electrostatic potentials, as well as free energies of
 *           binding and solvation.  It is invoked as:
 *
 *               apbs apbs.in
 *
 *           where apbs.in is a formatted input file (see documentation and
 *           examples).
 * 
 *  @version $Id$
 *  @attention
 *  @verbatim
 *
 * APBS -- Adaptive Poisson-Boltzmann Solver
 *
 * Nathan A. Baker (baker@biochem.wustl.edu)
 * Dept. of Biochemistry and Molecular Biophysics
 * Washington University in St. Louis
 *
 * Additional contributing authors listed in the code documentation.
 *
 * Copyright (c) 2002.  Washington University in St. Louis.
 * All Rights Reserved.
 *
 * Portions Copyright (c) 1999-2002.  The Regents of the University of
 * California.  
 * Portions Copyright (c) 1995.  Michael Holst.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and not-for-profit purposes,
 * without fee and without a signed licensing agreement, is hereby granted,
 * provided that the above copyright notice, this paragraph and the
 * following two paragraphs appear in all copies, modifications, and
 * distributions.
 *
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE
 * AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
 * HEREUNDER IS PROVIDED "AS IS".  THE AUTHORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * @endverbatim
 */

#include "apbscfg.h"
#include "apbs/apbs.h"  
#include "apbs/nosh.h"  
#include "apbs/mgparm.h"  
#include "apbs/pbeparm.h"  
#include "apbs/femparm.h"  

#include "routines.h"

VEMBED(rcsid="$Id$")

int main(int argc, char **argv) {

    NOsh *nosh = VNULL;
    MGparm *mgparm = VNULL;
    PBEparm *pbeparm = VNULL;
    Vmem *mem = VNULL;
    Vcom *com = VNULL;
    Vio *sock = VNULL;
    Vpmg *pmg[NOSH_MAXCALC];
    Vpmgp *pmgp[NOSH_MAXCALC];
    Vpbe *pbe[NOSH_MAXCALC];
    Valist *alist[NOSH_MAXMOL];
    Vgrid *dielXMap[NOSH_MAXMOL],*dielYMap[NOSH_MAXMOL],*dielZMap[NOSH_MAXMOL];
    Vgrid *kappaMap[NOSH_MAXMOL];
    Vgrid *chargeMap[NOSH_MAXMOL];
    char *input_path = VNULL;
    int i, rank, size, bytesTotal, highWater;

    /* These variables require some explaining... The energy double arrays
     * store energies from the various calculations.  The energy int array
     * stores a flag (0, 1, 2) that indicates whether no, total, or all
     * energies were evaluated for a particular calculation.  Likewise, the
     * force double arrays store forces from the various calcualtions.  The
     * force int array stores an integer which either says no calculation was
     * performed (0) or gives the number of entries in the force array for each
     * calculation */
    double qfEnergy[NOSH_MAXCALC], qmEnergy[NOSH_MAXCALC];
    double dielEnergy[NOSH_MAXCALC], totEnergy[NOSH_MAXCALC];
    double npEnergy[NOSH_MAXCALC];
    AtomForce *atomForce[NOSH_MAXCALC];
    int nenergy[NOSH_MAXCALC], nforce[NOSH_MAXCALC];
    /* THe real partition centers */
    double realCenter[3];

    /* Instructions: */
    char header[] = 
{"\n\n\
    ----------------------------------------------------------------------\n\
    Adaptive Poisson-Boltzmann Solver (APBS)\n\
    Version 0.2.1 (April 23, 2002)\n\
    \n\
    Nathan A. Baker (nbaker@wasabi.ucsd.edu)\n\
    Dept. of Chemistry and Biochemistry\n\
    Dept. of Mathematics, Scientific Computing Group\n\
    University of California, San Diego \n\n\
    Additional contributing authors listed in the code documentation.\n\n\
    Copyright (c) 1999-2002.  The Regents of the University of California.\n\
    Portions Copyright (c) 1995.  Michael Holst.\n\n\
    Permission to use, copy, modify, and distribute this software and its\n\
    documentation for educational, research, and not-for-profit purposes,\n\
    without fee and without a signed licensing agreement, is hereby granted,\n\
    provided that the above copyright notice, this paragraph and the\n\
    following two paragraphs appear in all copies, modifications, and\n\
    distributions.\n\n\
    IN NO EVENT SHALL THE AUTHORS BE LIABLE TO ANY PARTY FOR DIRECT,\n\
    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST\n\
    PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,\n\
    EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH\n\
    DAMAGE.\n\n\
    THE AUTHORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT\n\
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A\n\
    PARTICULAR PURPOSE.  THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF\n\
    ANY, PROVIDED HEREUNDER IS PROVIDED \"AS IS\".  THE AUTHORS HAVE NO\n\
    OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR\n\
    MODIFICATIONS.\n\
    ----------------------------------------------------------------------\n\
    \n\n"};
    char *usage = 
{"\n\n\
    ----------------------------------------------------------------------\n\
    This driver program calculates electrostatic potentials, energies,\n\
    and forces using both multigrid and finite element methods.\n\
    It is invoked as:\n\n\
      apbs apbs.in\n\n\
    where apbs.in is a formatted input file.\n\
    ----------------------------------------------------------------------\n\n"};


    /* ************** CHECK PARALLEL STATUS *************** */
    Vcom_init(&argc, &argv);
    com = Vcom_ctor(1);
    rank = Vcom_rank(com);
    size = Vcom_size(com);
    startVio(); 
    Vnm_setIoTag(rank, size);
    Vnm_tprint( 0, "Hello world from PE %d\n", rank);

    /* A bit of array/pointer initialization */
    mem = Vmem_ctor("MAIN");
    for (i=0; i<NOSH_MAXCALC; i++) {
        pmg[i] = VNULL;
        pmgp[i] = VNULL;
        pbe[i] = VNULL;
        qfEnergy[i] = 0;
        qmEnergy[i] = 0;
        dielEnergy[i] = 0;
        totEnergy[i] = 0;
        atomForce[i] = VNULL;
        nenergy[i] = 0;
        nforce[i] = 0;
    }
    for (i=0; i<NOSH_MAXMOL; i++) {
        alist[i] = VNULL;
        dielXMap[i] = VNULL;
        dielYMap[i] = VNULL;
        dielZMap[i] = VNULL;
        kappaMap[i] = VNULL;
        chargeMap[i] = VNULL;
    }

    /* *************** CHECK INVOCATION ******************* */
    Vnm_tstart(26, "APBS WALL CLOCK");
    Vnm_tprint( 1, "%s", header);
    Vnm_tprint( 1, "This executable compiled on %s at %s\n\n", __DATE__, 
      __TIME__);
    if (argc != 2) {
        Vnm_tprint(2, "ERROR -- CALLED WITH %d ARGUMENTS!\n", argc);
        Vnm_tprint(2, "%s\n", usage);
        return APBSRC;
    } 
    input_path = argv[1];


    /* *************** PARSE INPUT FILE ******************* */
    nosh = NOsh_ctor(rank, size);
    sock = Vio_ctor("FILE", "ASC", VNULL, input_path, "r");
    Vnm_tprint( 1, "Parsing input file %s...\n", input_path);
    if (!NOsh_parse(nosh, sock)) {
        Vnm_tprint( 2, "Error while parsing input file.\n");
        return APBSRC;
    } else Vnm_tprint( 1, "Parsed input file.\n");
    Vio_dtor(&sock);

    /* *************** LOAD MOLECULES ******************* */
    if (loadMolecules(nosh, alist) != 1) {
        Vnm_tprint(2, "Error reading molecules!\n");
        return APBSRC;
    }

    /* *************** LOAD MAPS ******************* */
    if (loadDielMaps(nosh, dielXMap, dielYMap, dielZMap) != 1) {
        Vnm_tprint(2, "Error reading dielectric maps!\n");
        return APBSRC;
    }
    if (loadKappaMaps(nosh, kappaMap) != 1) {
        Vnm_tprint(2, "Error reading kappa maps!\n");
        return APBSRC;
    }
    if (loadChargeMaps(nosh, chargeMap) != 1) {
        Vnm_tprint(2, "Error reading charge maps!\n");
        return APBSRC;
    }

    /* *************** DO THE CALCULATIONS ******************* */
    Vnm_tprint( 1, "Preparing to run %d PBE calculations.\n",
      nosh->ncalc);
    for (i=0; i<nosh->ncalc; i++) {
        Vnm_tprint( 1, "----------------------------------------\n");

        /* ***** Do MG calculation ***** */
        if (nosh->calc[i].calctype == 0) {

            Vnm_tprint( 1, "CALCULATION #%d: MULTIGRID\n", i+1);

            /* Useful local variables */
            mgparm = nosh->calc[i].mgparm;
            pbeparm = nosh->calc[i].pbeparm;

            /* Set up problem */
            Vnm_tprint( 1, "  Setting up problem...\n");
            if (!initMG(i, nosh, mgparm, pbeparm, realCenter, pbe, 
              alist, dielXMap, dielYMap, dielZMap, kappaMap, chargeMap, 
              pmgp, pmg)) {
                Vnm_tprint( 2, "Error setting up MG calculation!\n");
                return APBSRC;
            }

            /* Print problem parameters */
            printMGPARM(mgparm, realCenter);
            printPBEPARM(pbeparm);

            /* Solve PDE */
            if (solveMG(nosh, pmg[i], mgparm->type) != 1) {
                Vnm_tprint(2, "Error solving PDE!\n");
                return APBSRC;
            }

            /* Set partition information for observables and I/O */
            if (setPartMG(nosh, mgparm, pmg[i]) != 1) {
                Vnm_tprint(2, "Error setting partition info!\n");
                return APBSRC;
            }

            /* Write out energies */
            energyMG(nosh, i, pmg[i], &(nenergy[i]), 
              &(totEnergy[i]), &(qfEnergy[i]), &(qmEnergy[i]), 
              &(dielEnergy[i]));

            /* get apolar energy */
            npenergyMG(nosh, i, pmg[i], &(nenergy[i]),
                       &(npEnergy[i]));
	        npEnergy[i] = Vunit_kb*pbeparm->temp*(1e-3)*Vunit_Na*(npEnergy[i]);
	        Vnm_print(0, "Apolar energy: %f kJ/mol\n", npEnergy[i]);


            /* Write out forces */
            forceMG(mem, nosh, pbeparm, pmg[i], &(nforce[i]), 
              &(atomForce[i]), alist);

            /* Write out data folks might want */
            writedataMG(rank, nosh, pbeparm, pmg[i]);
            
            /* Write matrix */
            writematMG(rank, nosh, pbeparm, pmg[i]);

            fflush(stdout);
            fflush(stderr);

        /* ***** Do FEM calculation ***** */
        } else {
            Vnm_tprint( 2, "main: FEM shell support not implemented yet\n");
            return APBSRC;
        }
    } 

    
    /* *************** HANDLE PRINT STATEMENTS ******************* */
    if (nosh->nprint > 0) {
        Vnm_tprint( 1, "----------------------------------------\n");
        Vnm_tprint( 1, "PRINT STATEMENTS\n");
    }
    for (i=0; i<nosh->nprint; i++) {
        /* Print energy */
        if (nosh->printwhat[i] == 0) {
            printEnergy(com, nosh, totEnergy, i);
        /* Print force */
        } else if (nosh->printwhat[i] == 1) {
            printForce(com, nosh, nforce, atomForce, i);
        } else {
            Vnm_tprint( 2, "Undefined PRINT keyword!\n");
            break;
        }
    }
 

    /* *************** GARBAGE COLLECTION ******************* */
    Vnm_tprint( 1, "----------------------------------------\n");
    Vnm_tprint( 1, "CLEANING UP AND SHUTTING DOWN...\n");
    /* Clean up APBS structures */
    killForce(mem, nosh, nforce, atomForce);
    killEnergy();
    killMG(nosh, pbe, pmgp, pmg);
    killChargeMaps(nosh, chargeMap);
    killKappaMaps(nosh, kappaMap);
    killDielMaps(nosh, dielXMap, dielYMap, dielZMap);
    killMolecules(nosh, alist);
    NOsh_dtor(&nosh);

    /* Memory statistics */
    bytesTotal = Vmem_bytesTotal();
    highWater = Vmem_highWaterTotal();
    Vnm_tprint( 1, "Final memory usage:  %4.3f MB total, \
%4.3f MB high water\n", (double)(bytesTotal)/(1024.*1024.),
      (double)(highWater)/(1024.*1024.));

    /* Clean up MALOC structures */
    Vcom_dtor(&com);
    Vmem_dtor(&mem);

    /* And now it's time to so "so long"... */
    Vnm_print(1, "\n\n");
    Vnm_tprint( 1, "Thanks for using APBS!\n\n");

    /* This should be last */
    Vnm_tstop(26, "APBS WALL CLOCK");
    Vcom_finalize();

    return 0;

}
