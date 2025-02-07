/***********************************************************************

 * Copyright 1995-2012 Regine Hock (Surface energy balance model)
 *                     Carleen Tijm-Reijmer (Sub-surface model)
 *
 * This file is part of the Distributed Energy Balance Model (DEBaM).
 *
 * DEBaM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DEBaM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DEBaM.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/
/*************************************************************/
/* PROGRAM  debam.c, formerly meltmod.c                          */
/*   DISTRIBUTED SNOW/ICE MELT MODEL BASED ON ENERGY BALANCE */
/*   CALCULATIONS INCLUDING OPTIONAL DISCHARGE CALCULATIONS  */
/*   5.3.1998, update 13 June 2013, renamed August, 2012         */
/*************************************************************/


#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>

#include "closeall.h"
#include "discharg.h"
#include "disopt.h"
#include "globcor.h"
#include "grid.h"
#include "initial.h"
#include "input.h"
#include "radiat.h"
#include "snowinput.h"
#include "snowmodel.h"
#include "skintemperature.h"
#include "turbul.h"
#include "userfile.h"
#include "writeout.h"
/* all function calls, first line */
#include "variab.h"      /* all global VARIABLES */

/************* MAIN ************************************************/

int main()

{
    energymethod = 1;   /*melt calculated by energy balance approach*/
    /*needed in certain functions, which are    */
    /*also used by the degree day model         */

    ratio = startratio;

    /*----------------------------------------------------------------*/
    /*** READ DATA FROM CONTROLING INPUT FILE / INITIALISATION     ****/
    /*----------------------------------------------------------------*/

    input_read();           /*** READ INPUT DATA ***/
    startinputdata();       /***OPEN AND READ GRID FILES AND CLIMATE DATA***/
    checkgridinputdata_ok(); /*check if grid input data ok*/
    startoutascii();        /***OPEN ASCII-OUTPUT FILES AND WRITE HEAS***/
    startspecificmassbalance(); /*OPEN FILE FOR SPECIFIC MASS BAL, MULTI-YEAR RUNS*/
    startarrayreserve();    /***STORAGE RESERVATION OF ARRAYS FOR EACH TIME STEP***/
    glacierrowcol();  /*FIND FIRST, LAST ROW AND COLUMN OF AREA CALCULATED IN DTM*/
    readclim();       /*** READ CLIMATE INPUT FIRST ROW = ONE TIME STEP ****/

    /* puts("ok1");  */

    if((winterbalyes == 1) || (summerbalyes == 1))
        areaelevationbelts();    /*number of grid cells per elevation band, for bn profiles*/

    if(maxmeltstakes > 0)
        startmeltstakes();    /*OUTPUT OF MELT OF SEVERAL LOCATIONS - COMPARISON WITH STAKES*/

    if(methodsnowalbedo >= 2)  /*albedo variable in time, generated by model*/
        inialbedoalt();    /*allocate start albedo to each grid cell*/

    if(snowfreeyes == 1)   /*OPEN FILE WITH TIME SERIES WITH NO. OF SNOWFREE PIXELS*/
        opensnowfree();

    if((datesfromfileyes == 1) &&  (winterbalyes == 1) && (summerbalyes == 1))
        readdatesmassbal();      /*READ DATES OF MASS BAL MEASUREMENTS FOR EACH YEAR*/

    if(readsnowalbedo==1)      /*read measured daily snow albedo*/
        readalbedo29();        /*specific to Storglaciaren application: 1994*/
    /*   readalbedo16(); */

    if(methodprecipinterpol == 2)    /*read precipitation index map once (constant in time)*/
        readprecipindexmap();        /*in turbul.c under precipitation*/

    if (disyes >= 1)        /*DISCHARGE SIMULATION REQUESTED, 1=discharge data, 2=no data*/
        startdischarg();     /*INITIALIZE DISCHARGE SIMULATION for both cases*/

    /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
    if (methodsurftempglac == 4) { /* surface temperature determined using snowmodel */
        initgrid();        /*initialise subsurface grid including temperature and density profiles */
        outputsubsurf();   /*write initial conditions for each layer to file*/
    }
    if (methodsurftempglac != 4) /*CHR added for stability of subsurface scheme*/
        factinter = 1;
    /*============================================================*/

    /*=============FOR EVERY TIME STEP ===============================================*/

    do {

        nsteps += 1;         /*number of time steps of period calculated*/
        if((zeit==24) && ((int)jd/daysscreenoutput == floor((int)jd/daysscreenoutput)))
            printf("\n  yr= %4.0f  jd = %4.2f   time =%3.0f",year,jd,zeit);   /*SCREEN OUTPUT*/

        if(do_out_area == 1)
            areameannull();  /* SPATIAL MEANS OF MODEL OUTPUT SET TO ZERO */

        /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
        if (factinter > 1)   /*in case of subintervals per time interval*/
            timesteporig = timestep;    /*variable timestep will be manipulated in interpolate()*/
        /*set to original value after computation for further use in discharg.c etc*/

        for (inter=1; inter <= factinter; inter++) { /*CHR added: for each subinterval */
            /* start interpolation between two time steps in case of use of subsurface model*/
            if (factinter > 1)
                interpolate();     /*linearly interpolate climate data for subintervals*/
            /*============================================================*/

            /******* INTERPOLATION OF AIR TEMPERATURE ******/
            tempinterpol(); /*** ELEVATION-DEPENDENT AIR TEMP INTERPOLATION ***/

            /****** RADIATION *****************************/
            if (directfromfile == 0)
                schatten();      /*** CALCULATE SHADE, CORRECTION FACTOR, DIRECT GRIDs ***/
            else
                readdirect();    /* READ DIRECT RADIATION (slope corrected) FROM FILE */

            if(methodglobal==1)     /*no separation into direct and diffus at climate station*/
                ratioglobal();        /*CORRECTION RATIO FOR GLOBAL RADIATION DUE TO CLOUDS */
            else {          /*separation into direct and diffus at climate station*/
                topofatmosphere();           /*needed for ratio global/topofatm*/
                splitdiffusedirectkiruna();  /*split meas global into direct and diffuse*/
                meanalbedo();    /*mean albedo of entire drainage basin for terrain diffuse rad*/
                diffusunobstructed();   /*correct station diffuse rad, so that unobstructed sky*/
                ratiodirectdirect();    /*ratio actual direct to potential direct at station*/
            }  /*endelse*/

            if(methodglobal == 1) {  /*if methodglobal=2, topofatm already calculated; cloud parameterization with G/Toa ratio*/
                if( ((methodsnowalbedo == 3) || (methodsnowalbedo == 5)) || (methodlonginstation == 6))
                    topofatmosphere();   /*needed to determine cloudiness*/
            }

            switch(methodlonginstation) {  /*HOW IS LONGWAVE INCOMING AT STATION DETERMINED*/
            case 1:
                longinstationnetradmeas();     /*FROM MEAS NET, GLOB, REF*/
                break;
            case 2:
                break;                   /*LONGIN READ FROM CLIMATE DATA INPUT FILE*/
                /*has been read into LWin in readclim*/
            case 3:
                longinstationkonzel();   /*PARAMETERIZATION Konzelmann et al. using cloud amount*/
                break;
            case 4:
                longinstationbrunts();
                break;
            case 5:
                longinstationbrutsaert();
                break;
            case 6:
                longinstationkonzel();   /*as 3, but clouds are parameterized, function called there*/
                break;
            default:
                puts("\n no choice of methodlonginstation made\n\n");
                exit(3);
                break;
            }  /*end switch*/


            /*REMOVE TOPOGRAPHIC INFLUENCE ON CLIMATE STATION LONGWAVE INCOMING
              ONLY IF CLIMATE STATION NOT OUSIDE AREA TO BE CALCULATED*/
            if((methodlongin == 2) && (climoutsideyes == 0))    /*LONGWAVE BASED ON PLUESS, 1997*/
                longinskyunobstructed();  /*LONGWAVE IN AT CLIMATE STATION IF IT WAS UNOBSTRUCTED*/

            if(methodinisnow == 1)   /*DISTRIBUTION SNOW/ICE PRESCRIBED, READ FROM FILES*/
                albedoread();   /*OPEN AND READ SURFACE TYPE FILE IF NEW ONE VALID FOR TIME STEP*/
            /* integer for each surface type in array surface */
            else
                whichsurface(); /*VALUE FOR ARRAY surface (SNOW,FIRN,ICE,ROCK) FOR ALBEDO, K-VALUES*/


            /*SWBAL is needed in order to calculate skin layer temperature at the climate station*/
            if (methodsurftempglac == 4 && skin_or_inter == 0 ) {    
                i=rowclim;
                j=colclim;   
            /********* GLOBAL RADIATION **********************/
            if(methodglobal==1)     /*no separation into direct and diffus*/
                globradratio();           /*calculation of global radiation*/
            if(methodglobal==2) {   /*separate interpolation of direct and diffuse radiation*/
                interpoldirect();
                interpoldiffus();
                adddirectdiffus();
            }

           /********** PRECIPITATION *****needed before albedo in method 2*********/
            precipinterpol();
            precipenergy(); 

            /********* ALBEDO **********************/
             if(readsnowalbedo==0) { /*no use of albedo measurements*/
                 switch(methodsnowalbedo) {
                 case 1:
                     albedocalcconst();    /*constant albedo for snow/slush/ice*/
                     break;
                 case 2:
                     albedocalc();  /*albedo generated as function of T, snow fall*/
                     break;
                 case 3:
                     albedocalc();  /*as 2 but incl. cloud dependence*/
                     break;
                 case 4:
                     albedocalcdepth();  /*as 2 but depending on snowdepth*/
                     break;
                 case 5:
                     albedocalcdepth();  /*as 4 but incl. cloud dependence*/
                     break;
                 case 6:
                     albedosnowpoly();  /*modified version of oerlemans and knap, sicart PhD. p.243*/
                     break;
                 }  /*end case*/
             }  /*endif*/
             else     /*measured albedo data read from file*/
                 albedosnowmeas();   /*use measured daily means of snow albedo - Storglac*/

             shortwavebalance();    /*SHORTWAVE RADIATION BALANCE*/
             }/*end extra actions to create correct input for skin layer formulation

            /*LONGWAVE OUT RADIATION AT CLIMATE STATION FROM MEAS*/
            if(methodsurftempglac == 3) { /*use longwave outgoing measurements at climate station*/
                i=rowclim;
                j=colclim;   /*needed because surtempfromlongout also used later for entire grid*/
                surftempfromlongout();       /*CALCULATE SURFACE TEMP AT CLIMATE STATION*/
            } else
                surftemp[rowclim][colclim] = 0.;

            if(methodprecipinterpol == 3)   /*read precipitation grids from file for each time step*/
                readprecipfromfile();

            /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
            if(methodsurftempglac == 4) { /*CHR added option*/
                i=rowclim;
                j=colclim;
                surftempfrommodel();  /*CALCULATE SURFACE TEMP AT CLIMATE STATION FROM T OF 2 UPPER LAYERS*/
                /* in case of skin layer formulation extrapolation used as first guess*/
                if (skin_or_inter == 0) {
                LONGIN[i][j] = LWin;
                surftempskin(); /*CALCULATE SURFACE TEMP BASED ON SKIN LAYER FORMULATION*/
                }
                surftempstationalt=surftemp[rowclim][colclim];
                if (((int)jd2 == (int)summerjdend+1) && ((int)zeit == 1) && (inter == 1)) {
                    resetgridwinter();
                }
                if (((int)jd2 == (int)winterjdend+1) && ((int)zeit == 1) && (inter == 1))
                    resetgridsummer();
            }

            /*============================================================*/

            /*CLIMATE STATION GRID MUST BE COMPUTED FIRST, IN ORDER TO CALCULATE THE LONGWAVE INCOMING
              RADIATION FOR CLIMATE STATION IN CASE OF SPATIALLY DISTRIBUTED, BEFORE INTERPOLATING*/
            /*ONLY POSSIBLE IF STATION IS LOCATED ON GLACIER, OTHERWISE NO ITERATION FOR STATION GRID
               - USE LONGIN AS DETERMINED FROM MEASUREMENTS FOR EXTRAPOLATION*/
            if((methodsurftempglac == 2) || ((methodsurftempglac == 4) && (griddgmglac[rowclim][colclim] != nodata)))
                iterationstation();    /*CALCULATE ENERGY BALANCE AT CLIMATE STATION*/

            /*IF NO SURFACE TEMP ITERATION THERE IS NO COMPUTATION OF STABILITY FUNCTIONS FOR
              LOCATION OF CLIMATE STATION, THEREFORE FIRST COMPUTATON OF SENSIBLE HEAT FLUX
              FOR CLIMATE STATION */
            if((methodsurftempglac != 2) && (methodsurftempglac != 4)) {
                i=rowclim;
                j=colclim;    /*ONLY AT CLIMATE STATION GRID CELL*/
                if (methodturbul == 3) { /*CHR*/
                    airpress();    /*** CALCULATION AIR PRESSURE AT GRID POINT FOR TURB FLUXES ***/
                    vappress();    /*** CALCULATION VAPOUR PRESSURE FROM REL. HUMIDITY  ***/
                    sensiblestabilityiteration();    /*DETERMINE STABILITY FUNCTIONS AND z0T, zoe*/
                }
                if ((methodturbul == 4) && (skin_or_inter == 1)) /*by C. Tijm-Reijmner*/
                    turbfluxes();  /*almost same as 3, but differently programmed*/
            }  /*endif*/


            /*================================================================================*/
            /*------- FOR EACH GRID POINT - only for grid cells defined by dgmdrain ----------*/
            /*================================================================================*/

            if(calcgridyes == 2) {    /*computation only for climate station grid cell*/
                firstrow = rowclim;
                lastrow = rowclim;
                firstcol[firstrow] = colclim;
                lastcol[firstrow] = colclim;
            }      /*will only go through grid loop once for climate station grid cell*/

            for (i=firstrow; i<=lastrow; i++)         /*for all rows of drainage basin grid*/
                for (j=firstcol[i]; j<=lastcol[i]; j++) { /*for all columns*/
                    if (griddgmdrain[i][j] != nodata) { /*only for area to be calculated*/
                        if (griddgmdrain[i][j] != griddgm[i][j]) {
                            printf("\n\n ERROR elevation in DTM is not the same as in glacier grid\n");
                            printf(" row  %d  col  %d      (in main) \n\n",i,j);
                            exit(12);
                        }

                        if(((methodsurftempglac == 2) || (methodsurftempglac == 4)) && (i==rowclim) && (j==colclim))
                            notcalc = 1;    /*ENERGY BALANCE FOR STATION GRID CELL ALREADY CALCULATED*/
                        else               /*TO AVOID TO BE COMPUTED AGAIN*/
                            notcalc = 0;

                        if(notcalc==0) {    /*COMPUTE ENERGY BALANCE ONLY IF NOT YET CALCULATED*/
                            /********* GLOBAL RADIATION **********************/
                            if(methodglobal==1)     /*no separation into direct and diffus*/
                                globradratio();           /*calculation of global radiation*/
                            if(methodglobal==2) {   /*separate interpolation of direct and diffuse radiation*/
                                interpoldirect();
                                interpoldiffus();
                                adddirectdiffus();
                            }

                            /********** PRECIPITATION *****needed before albedo in method 2*********/
                            precipinterpol();
                            precipenergy();      /*rain outside glacier considered after grid computed*/

                            /********* ALBEDO **********************/
                            if(readsnowalbedo==0) { /*no use of albedo measurements*/
                                switch(methodsnowalbedo) {
                                case 1:
                                    albedocalcconst();    /*constant albedo for snow/slush/ice*/
                                    break;
                                case 2:
                                    albedocalc();  /*albedo generated as function of T, snow fall*/
                                    break;
                                case 3:
                                    albedocalc();  /*as 2 but incl. cloud dependence*/
                                    break;
                                case 4:
                                    albedocalcdepth();  /*as 2 but depending on snowdepth*/
                                    break;
                                case 5:
                                    albedocalcdepth();  /*as 4 but incl. cloud dependence*/
                                    break;
                                case 6:
                                    albedosnowpoly();  /*modified version of oerlemans and knap, sicart PhD. p.243*/
                                    break;
                                }  /*end case*/
                            }  /*endif*/
                            else     /*measured albedo data read from file*/
                                albedosnowmeas();   /*use measured daily means of snow albedo - Storglac*/

                            shortwavebalance();    /*SHORTWAVE RADIATION BALANCE*/

                            /********* ROCK SURFACE TEMPERATURE **********************/
                            /*  if(methodlongin == 2)    LONGWAVE ASSUMED VARIABLE, BASED ON PLUESS, 1997*/
                            /*    tempsurfacerock();     SURFACE TEMPERATURE OF ROCK OUTSIDE GLACIER*/

                            /*CASE 2: before iteration surftemp must be reset to 0, strictly speaking, only if lowered via iteration,
                              or if surftemp written to output (because array is overwritten for output means*/
                            /* at start in initial.c set to 0 for area calculated*/
                            switch(methodsurftempglac) {
                            case 1:  /*NO ITERATION, TEMP CONSTANT AS AT CLIMATE STATION*/
                                surftemp[i][j] = surftemp[rowclim][colclim];
                                break;
                            case 2:  /*ITERATION SO THAT ENBAL IS BALANCED, START WITH TEMP=0, THEN LOWER TEMP*/
                                surftemp[i][j] = 0;
                                break;
                            case 3:
                                if(surftemplapserate == 0)	  /* measured surf temp constant in space*/
                                    surftemp[i][j] = surftemp[rowclim][colclim];
                                else {	                  /* measured surf temp decreases with elevation, lapserate change per 100 m */
                                    surftemp[i][j] = surftemp[rowclim][colclim] + (griddgm[i][j]-griddgm[rowclim][colclim])/100 * surftemplapserate;
                                    if(surftemp[i][j] > 0)
                                        surftemp[i][j] = 0;
                                }
                                break;
                            case 4:  /*SNOW MODEL*/
                                surftempfrommodel();   /*surftemp from interpolation of T of upper 2 layers*/
                                 /* in case of skin layer formulation extrapolation used as first guess*/
                                if (skin_or_inter == 0) {
                                if(methodlongin == 2) {     /*LONGWAVE INCOMING RADIATION VARIABLE IN SPACE*/
                                    longinpluess();   }        /*NEEDS SURFACE TEMPERATURE in this case from previous time step*/
                                else
                                { LONGIN[i][j] = LWin;}
                                surftempskin(); /*CALCULATE SURFACE TEMP BASED ON SKIN LAYER FORMULATION*/
                                }
                                break;
                            }


                            airpress();    /*** CALCULATION AIR PRESSURE AT GRID POINT FOR TURB FLUXES ***/
                            vappress();    /*** CALCULATION VAPOUR PRESSURE FROM REL. HUMIDITY  ***/

                            /**** ======== ITERATION LOOP FOR SURFACE TEMP ICE, SNOW ============================ */
                            /**** longwave outgoing, turbulent fluxes and longwave incoming (if according to Pluess) are
                                  affected by surface temperature and thus calculated inside iteration loop
                                  effect of changing surface temp on rain energy neglected ***/
                            do {        /*goes only once through the loop in case methodsurftempglac is not 2*/
                                /************* TURBULENT FLUXES *********************/
                                if(methodsurftempglac >= 2)   /*surface temp may change*/
                                    vappressicesnow();       /*saturation vapour pressure of ice, snow surface*/

                                switch(methodturbul) {
                                case 1:
                                    sensescher();    /*ACCORDING TO ESCHER-VETTER*/
                                    latescher();
                                    break;
                                case 2:
                                    sensible();   /*NO STABILITY, STAB FUNCTIONS = 0*/
                                    latent();
                                    break;
                                case 3:
                                    sensible();   /*INCLUDING STABILITY, SAME FUNCTIONS AS 2 BUT*/
                                    latent();     /*STAB FUNCTIONS HAVE BEEN DETERMINED BEFORE */
                                    break;        /*STAB FUNCTIONS SPATIALLY NOT VARIABLE*/
                                case 4:
                                    if (skin_or_inter == 1) turbfluxes();  /*as 3 but different way: Carleen Tijm-Reijmer, 2/2005*/
                                    break;
                                }


                                /********* LONGWAVE RADIATION **********************/
                                if(methodlongin == 2) {     /*LONGWAVE INCOMING RADIATION VARIABLE IN SPACE*/
                                   if ((methodsurftempglac != 4) || (skin_or_inter == 1))
                                    longinpluess();           /*NEEDS SURFACE TEMPERATURE*/
                                }

                                if(methodsurftempglac >= 2)     /*LONGWAVE OUTGOING RAD VARIABLE IN SPACE OR FROM LONGOUT MEAS*/
                                    longoutcalc();    /*if melting surface: LWout is initialized to melting conditions*/

                                /********* RADIATION BALANCE **********************/
                                radiationbalance();

                                /*  for Keikos article:  NETRAD[rowclim][colclim] = glob - ref + LWin - LWout; */


                                /********* ICE HEAT FLUX **************************/
                                if(methodiceheat == 2)      /*if 1 no heat flux*/
                                    iceheatStorglac();        /*predefined ice heat flux, specific Storglac*/

                                /********* ENERGY BALANCE *************************/
                                if (methodsurftempglac == 4 && skin_or_inter == 1)
                                    ICEHEAT[i][j] = 0.;

                                energybalance();

                                if((methodnegbal == 2) && (iternumber == 0))
                                    negenergybalance();      /*STORE NEGATIVE ENERGY BALANCE*/
                                /*iternumber must to 0 to ensure that in case of iteration, the first
                                  negative ENBAL is stored  before it is brought to zero by lowering the surface
                                  temperature in the iteration procedure (the function is called only once for every
                                  time step and each grid cell), iternumber is increased when surface temp lowered,
                                  after iteration loop energy balance can not be negative any more*/

                                if((methodsurftempglac == 2) && (ENBAL[i][j] < 0)) {  /*iteration wanted*/
                                    if(surface[i][j] != 4)  /*glacier or snow on rock*/
                                        surftemp[i][j] -= iterstep;      /*decrease temperature*/
                                    iternumber += 1;
                                    /*count number of iteration steps*/
                                }  /*endif*/

                                if(methodsurftempglac == 2)    /*iterationend initialized to 10 in variab.h*/
                                    iterationend = ENBAL[i][j];  /*trick to determine iterationend: if pos no further iteration*/

                                if(iternumber > 1000) {   /*to avoid endless loops*/
                                    printf("\n\nTOO MANY ITERATIONS (<1000)  jd=%4.1f %5.1f row %5d col %5d\n\n",jd,zeit,i,j);
                                    exit(20);
                                }

                                if(surface[i][j] == 4)    /*no iteration for rock surface*/
                                    iterationend = 10;      /*energy balance only on glacier and on snow of interest*/
                                if(surftemp[i][j] < surftempminimum)     /*stop iteration to avoid surface temp too low*/
                                    iterationend = 10;

                            } while(iterationend < 0);   /*is set > 0 if ENBAL is positive and thus loop exited*/
                            /**** ======== ITERATION LOOP FOR SURFACE TEMP ICE, SNOW end: next temp ===== */

                            if(iternumber > 0) {
                                ENBAL[i][j] = 0;   /*set to 0, so that it is not pos after iteration is over*/
                                iternumber = 0;     /*set to zero for next grid cell*/
                                iterationend = 10;  /*set positive, to avoid iteration if ENBAL pos*/
                            }

                            /****** WATER EQUIVALENT MELT/ABLATION ******/
                            /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
                            /*before only waterequi-functions called*/
                            if ((methodsurftempglac != 4) || ((methodsurftempglac == 4) && (percolationyes == 0))) { /*CHR added*/
                                waterequivalentmelt();     /*** WATER EQUIVALENT MELT ***/
                                waterequivalentabla();     /*** WATER EQUIVALENT ABLATION ***/
                                RUNOFF[i][j] = MELT[i][j] + rainprec;
                            } else
                                MELT[i][j] = 0.0;

                            if (methodsurftempglac == 4) {
                                if (/*(percolationyes == 1) &&*/ (inter == 1)) { /*only first subtime step*/
                                    MELTsum[i][j] = 0.;
                                    ABLAsum[i][j] = 0.;
                                    RUNOFFsum[i][j] = 0.;
                                    SNOWsum[i][j] = 0.;
                                }
                                subsurf(); /*chr calculate new surface temperature field*/
                                waterequivalentabla();     /*** WATER EQUIVALENT ABLATION ***/
                                /* if (percolationyes == 1)*/
                                {
                                    ABLAsum[i][j] += ABLA[i][j];
                                    MELTsum[i][j] += MELT[i][j];
                                    RUNOFFsum[i][j] += RUNOFF[i][j];
                                    SNOWsum[i][j] += snowprec;
                                    sumSNOWprec[i][j] += snowprec;
                                    sumRAINprec[i][j] += rainprec;
                                }
                            }  /*endif method*/
                            /*============================================================*/

                            if(methodinisnow == 2)
                                snowcover();    /*compute how much snow is left*/

                            /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
                            if (methodsurftempglac == 4) {
                                changegrid();
                                if ((inter == factinter) &&
                                        ((int)((offsetsubsurfout+zeit)/factsubsurfout) == (offsetsubsurfout+zeit)/factsubsurfout))
                                    outputsubsurf();
                                if ((inter == factinter) && ((int)jd2 == (int)jdsurface[newday]) &&
                                        ((int)zeit == offsetsubsurfout) && (calcgridyes == 1))
                                    outputsubsurflines();
                            }
                            /*============================================================*/

                                if (griddgmglac[i][j] != nodata)   /*only for glacier, no matter if dgmdrain is larger*/
                                    massbalance();

                            /*============================================================*/

                            /*      if ((methodsurftempglac == 4) && (percolationyes == 1) && (inter == factinter)) */
                            if ((methodsurftempglac == 4) && (inter == factinter)) {
                                MELT[i][j] = MELTsum[i][j];
                                ABLA[i][j] = ABLAsum[i][j];
                                RUNOFF[i][j] = RUNOFFsum[i][j];
                            }

                            /********* OUTPUT ****/
                            if((do_out_area == 1) && (inter == factinter))  /*CHR added */
                                areasum();     /*** SUMMING UP ALL VALUES OVER AREA - for spatial means ***/

                        } /*endif notcalc*/

                    } /*endif griddgmdrain not nodata*/
                }  /*endfor next grid*/
        }/*END SUBTIMESTEP loop  for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005*/

        /***------------------ NEXT GRID --------------------------------------------- ***/


        /*======= for SNOWMODEL by Carleen Tijm-Reijmer, 2/2005=======*/
        /*set back timestep to original value to be used in temporal mean calculations
          and in discharge functions; it was manipulated in interpolate()*/
        if (factinter > 1)
            timestep = timesteporig;
        /*============================================================*/


        if(methodglobal == 2)     /* direct and diffuse radiation separate */
            meanalbedo();    /*needed for extrapolation of diffuse radiation */
        /*needed every timestep, the value calculated for this time step will be used
          for the next time step, because otherwise, albedo and diffuse radiation could
          not be computed within the same grid cell loop, because the mean is needed
          for each grid cell before albedo has been calculated for the entire grid*/


        /*surftemp and longout may change by iteration, functions longinstationmeas and
          longinskyunobstructed are outside iteration loop, therefore the values obtained
          after iteration are used for next time step in these functions*/
        /*  if(methodsurftempglac == 2)
            surftempstationalt = surftemp[rowclim][colclim];
          if((methodlonginstation == 1) && (methodsurftempglac == 2))
            LWout = LONGOUT [rowclim][colclim];   */


        /*WHOLE GRIDS ARE CALCULATED FOR ONE TIME STEP -
            NOW CALCULATE DISCHARGE AND WRITE TO OUTPUTFILE*/
        /********************* DISCHARGE ************************/
        if (disyes >= 1) {   /*DISCHARGE TO BE CALCULATED, measured file available (1) or not (2)*/
            if(onlyglacieryes == 1)     /*drainage basin larger than glacier*/
                rainoutsideglac();      /*rain from outside put proportionally onto the glacier*/

            if (disyesopt == 0)      /*simulation run, no optimization*/
                discharge();    /*for every grid calculated discharge, sum to melt and sum discharge*/
            else                     /*optimization run*/
                dischargeopt();       /*calculate discharge for each parameter set*/
        }

        /*** WRITE MODEL RESULTS TO FILE FOR INDIVIDUAL GRIDPOINTS FOR EVERY TIME STEP ***/
        if (outgridnumber > 0)     /*output file requested by user*/
            stationoutput();


        /*** WRITE MELT FOR SEVERAL LOCATIONS TO ONE FILE ***/
        if(maxmeltstakes > 0)
            writemeltstakes();


        /*** WRITE GRID FILES ***/
        switch(do_out)   /*** WRITE ENERGY BALANCE GRID-OUTPUT-FILES ***/

        {
        case 0:
            break;
        case 1:     /*OUTPUT EVERY TIME STEP*/
            startwritehour();    /*open output-files for every time step*/
            writegridoutput();
            break;

        case 2:     /*OUTPUT ONLY EVERY DAY*/
            sumday();             /*sum up values for subdiurnal timesteps*/
            if (zeit == 24.0) {   /*last hour of day*/
                startwriteday();   /*open files for daily means*/
                writegridoutput(); /*write grid to output file*/
                meandaynull();     /*after writing initialize to zero*/
                if (methodsurftempglac == 4) meandaysnownull();
            }  /*if*/
            break;

        case 3:     /*OUTPUT ONLY FOR WHOLE PERIOD*/
            sumall();             /*sum up values for subdiurnal timesteps*/
            break;                /*mean for whole period : write at end*/

        case 4:     /*OUTPUT EVERY DAY AND FOR WHOLE PERIOD*/
            sumday();             /*sum up for daily means*/
            sumall();
            /*must be done before writing daily means to files, because
             MELT-array will be overwritten by daily mean, discharge must
             also be calculated before*/

            if (zeit == 24) {     /*daily means, write period means at end*/
                startwriteday();   /*open files for daily means*/
                writegridoutput();
                meandaynull();     /*after writing initialize to zero*/
                if (methodsurftempglac == 4) meandaysnownull();
            }  /*if*/
            break;
        } /*switch*/

        /*CHECK IF SURFACE CONDITIONS OR SNOW COVER FILES SHOULD BE WRITTEN TO OUTPUT*/
        /*    NO TEMPORAL MEANS POSSIBLE - FOR VALIDATION OF SNOW LINE RETREAT */
        writesnoworsurfaceyes();

        /* WRITE GRID OF SURFACE CONDITIONS - ONLY FOR MIDNIGHT EVERY daysnow-th DAY */
        /*   OR FOR SELECTED DAYS SPECIFIED IN input.dat*/

        if((surfyes >= 1) && (write2fileyes == 1) && (calcgridyes == 1))
            writesurface();      /*open file and write to file*/
        if((snowyes >= 1) && (write2fileyes == 1) && (calcgridyes == 1))
            writesnowcover();      /*open file and write to file*/

        /*  WRITE TO TIME SERIES ASCII FILE EVERY MIDNIGHT HOW MANY PIXELS SNOWFREE*/
        if((snowfreeyes == 1) && (zeit == 24) && (calcgridyes == 1))
            percentsnowfree();

        /* WRITE TIME SERIES OF SPATIAL MEAN MODEL RESULTS TO OUTPUT FOR EVERY TIME STEP*/
        if (do_out_area == 1)
            areameanwrite();

        /*write winter/summer/mass balance grids at end of winter/summer*/
        if((winterbalyes == 1) || (summerbalyes == 1))
            if((zeit == 24) && (calcgridyes == 1))    /*check at end of end*/
                writemassbalgrid();

        /*set snow array to zero at end of melt season for next mass balance year*/
        /*to avoid that snow constantly accumulates in accumulation area and therefore*/
        /*firn is never exposed; done each year at start of winter*/
        /*  if( (methodinisnow == 2) && (snow2zeroeachyearyes == 1) && (jd == (winterjdbeg-15)) && (zeit == 24) )*/
        if( (methodsurftempglac != 4) && (methodinisnow == 2) && (snow2zeroeachyearyes == 1) && (jd == (winterjdbeg)) && (zeit == 24))
            initializeglacier2zero_nodatadouble(nrows, ncols, SNOW);

        readclim();       /*** READ CLIMATE INPUT NEXT TIME STEP ****/

        /*DEFINE WHEN TO EXIT THE LOOP*/
        if(timestep != 24) {  /*subdaily timesteps: midnight row is next julian day not to continue with*/
            if((jd > (jdend+1)) && (year == yearend))
                stoploop = 1;
        } else   /*DAILY TIME STEPS*/
            if((jd == jdend+1) && (year == yearend))   /*otherwise last julian day would not be run*/
                stoploop = 1;

    }  while (stoploop != 1);
    /*====================== NEXT TIME STEP =======================================*/


    /*OUTPUT OF MEAN COMPONENTS OF ENERGY BALANCE FOR WHOLE PERIOD OF CALCULATION*/

    if ((do_out == 3) || (do_out == 4)) {   /*mean of whole period*/
        if (calcgridyes == 1) {
            startwriteall();   /*open files for whole period*/
            writegridoutput();
        }
    }

    /********** WRITE MEAN MASS BALANCE PROFILE TO FILE********************************/
    if(((winterbalyes == 1) || (summerbalyes == 1)) && (yearend > yearbeg))
        meanmassbalprofile();

    /******************************************************************/

    if (disyes == 1) {    /*only if discharge data available*/
        r2calc();
        r2calcln();
        if (disyesopt == 1)   /*optimization run*/
            write2matriz();    /*write r2 matriz to file*/
    }

    closeall();     /* CLOSE FILES, FREE STORAGE */

    printf("\n\n number of glacier grids         %d\n\n",nglac);
    printf(" number of calculated time steps               %d\n",nsteps);
    printf(" number of timesteps of discharge data available  %d\n\n",nstepsdis);
    printf(" output written to   %s\n\n",outpath);
    if((methodsurftempglac == 3) || (methodsurftempglac == 4))
        printf("\n unrealistic values reached = %d times (resoutlines) \n\n",resoutlines);

    printf("********* PROGRAM RUN COMPLETED ************\n\n");

    return 0;
}

