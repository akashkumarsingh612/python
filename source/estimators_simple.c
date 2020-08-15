/***********************************************************/
/** @file  estimators_simple.c
 * @author ksl
 * @date   July, 2020   
 *
 * @brief  Routines related to updating estimators for the
 * case of simple atoms.  The routine is primarily intended
 * for simple atoms, but also may be called when a macro-atom
 * calculation is carried out.
 *
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "atomic.h"
#include "python.h"


/**********************************************************/
/** 
 * @brief      updates the estimators required for determining crude
 * spectra in each Plasma cell
 *
 * @param [in,out] PlasmaPtr  xplasma   PlasmaPtr for the cell of interest
 * @param [in] PhotPtr  p   Photon pointer in CMF frame
 * @param [in] double  ds   ds travelled in CMF frame
 * @param [in] double  w_ave   the weight of the photon in CMF frame 
 *
 * @return  Always returns 0
 *
 *
 *
 * @details
 * 
 * Increments the estimators that allow one to construct a crude
 * spectrum in each cell of the wind.  The frequency intervals
 * in which the spectra are constructed are in geo.xfreq. This information
 * is used in different ways (or not at all) depending on the ionization mode.
 *
 * It also records the various parameters intended to describe the radiation field, 
 * including the IP.
 *
 * ### Notes ###
 *
 * The term direct refers to photons that have not been scattered by the wind.
 * 
 * In non macro atom mode, w_ave
 * this is an average weight (passed as w_ave), but 
 * in macro atom mode weights are never reduced (so p->w 
 * is used).
 *
 * This routine is called from bf_estimators in macro_atom modes
 * and from radiation.  Although the historical documentation
 * suggests it is only called for certain ionization modes, it appears
 * to be called in all cases, though clearly it is only provides diagnostic
 * information in some of them.
 *
 * XFRAME - update_banded_estimators takes CMF inputs
 * The photon frequency and number are used.
 * 
 **********************************************************/


/* A couple of external variables to improve the counting of ionizing
   photons coming into a cell
*/
int nioniz_nplasma = -1;
int nioniz_np = -1;

/* A couple of external variables to improve the counting of photons
   in a cell
*/

int plog_nplasma = -1;
int plog_np = -1;

int
update_banded_estimators (xplasma, p, ds, w_ave, ndom)
     PlasmaPtr xplasma;
     PhotPtr p;
     double ds;
     double w_ave;
     int ndom;
{
  int i;

  /*photon weight times distance in the shell is proportional to the mean intensity */

  xplasma->j += w_ave * ds;

  if (p->nscat == 0)
  {
    xplasma->j_direct += w_ave * ds;
  }
  else
  {
    xplasma->j_scatt += w_ave * ds;
  }



/* frequency weighted by the weights and distance in the shell .  See eqn 2 ML93 */
  xplasma->mean_ds += ds;
  xplasma->n_ds++;
  xplasma->ave_freq += p->freq * w_ave * ds;

  /* The next loop updates the banded versions of j and ave_freq, analogously to routine inradiation
     nxfreq refers to how many frequencies we have defining the bands. So, if we have 5 bands, we have 6 frequencies, 
     going from xfreq[0] to xfreq[5] Since we are breaking out of the loop when i>=nxfreq, this means the last loop 
     will be i=nxfreq-1 */

  /* note that here we can use the photon weight and don't need to calculate anm attenuated average weight
     as energy packets are indisivible in macro atom mode */


  for (i = 0; i < geo.nxfreq; i++)
  {
    if (geo.xfreq[i] < p->freq && p->freq <= geo.xfreq[i + 1])
    {
      xplasma->xave_freq[i] += p->freq * w_ave * ds;    /* frequency weighted by weight and distance */
      xplasma->xsd_freq[i] += p->freq * p->freq * w_ave * ds;   /* input to allow standard deviation to be calculated */
      xplasma->xj[i] += w_ave * ds;     /* photon weight times distance travelled */
      xplasma->nxtot[i]++;      /* increment the frequency banded photon counter */
      /* work out the range of frequencies within a band where photons have been seen */
      if (p->freq < xplasma->fmin[i])
      {
        xplasma->fmin[i] = p->freq;
      }
      if (p->freq > xplasma->fmax[i])
      {
        xplasma->fmax[i] = p->freq;
      }

    }
  }

  /* NSH 131213 slight change to the line computing IP, we now split out direct and scattered - this was 
     mainly for the progha_13 work, but is of general interest */
  /* 70h -- nsh -- 111004 added to try to calculate the IP for the cell. Note that 
   * this may well end up not being correct, since the same photon could be counted 
   * several times if it is rattling around.... */

  /* 1401 JM -- Similarly to the above routines, this is another bit of code added to radiation
     which originally did not get called in macro atom mode. */

  /* NSH had implemented a scattered and direct contribution to the IP. This doesn't really work 
     in the same way in macro atoms, so should instead be thought of as 
     'direct from source' and 'reprocessed' radiation */

  if (xplasma->nplasma != plog_nplasma || p->np != plog_np)
  {
    xplasma->ntot++;

    /* NSH 15/4/11 Lines added to try to keep track of where the photons are coming from, 
     * and hence get an idea of how 'agny' or 'disky' the cell is. */
    /* ksl - 1112 - Fixed this so it records the number of photon bundles and not the total
     * number of photons.  Also used the PTYPE designations as one should as a matter of 
     * course
     */

    if (p->origin == PTYPE_STAR)
      xplasma->ntot_star++;
    else if (p->origin == PTYPE_BL)
      xplasma->ntot_bl++;
    else if (p->origin == PTYPE_DISK)
      xplasma->ntot_disk++;
    else if (p->origin == PTYPE_WIND)
      xplasma->ntot_wind++;
    else if (p->origin == PTYPE_AGN)
      xplasma->ntot_agn++;
    plog_nplasma = xplasma->nplasma;
    plog_np = p->np;
  }







  if (HEV * p->freq > 13.6)     // only record if above H ionization edge
  {

    /*
     * Calculate the number of H ionizing photons, see #255
     * EP 11-19: moving the number of ionizing photons counter into this
     * function so it will be incremented for both macro and non-macro modes
     */
    if (xplasma->nplasma != nioniz_nplasma || p->np != nioniz_np)
    {
      xplasma->nioniz++;
      nioniz_nplasma = xplasma->nplasma;
      nioniz_np = p->np;
    }

    /* IP needs to be radiation density in the cell. We sum contributions from
       each photon, then it is normalised in wind_update. */
    xplasma->ip += ((w_ave * ds) / (PLANCK * p->freq));

    if (HEV * p->freq < 13600)  //Tartar et al integrate up to 1000Ryd to define the ionization parameter
    {
      xplasma->xi += (w_ave * ds);
    }

    if (p->nscat == 0)
    {
      xplasma->ip_direct += ((w_ave * ds) / (PLANCK * p->freq));
    }
    else
    {
      xplasma->ip_scatt += ((w_ave * ds) / (PLANCK * p->freq));
    }
  }




  return (0);
}

/**********************************************************/
/** 
 * @brief      updates the estimators for calculating
 * the radiative force on each Plasma Cell
 * spectra in each Plasma cell
 *
 * @param [in,out] PlasmaPtr  xplasma   PlasmaPtr for the cell of interest
 * @param [in] PhotPtr  phot_mid   Photon pointer at midpt in Observer frame
 * @param [in] double  ds   ds travelled in Observer frame
 * @param [in] double  w_ave   the weight of the photon in Observer frame
 *
 * @return  Always returns 0
 *
 *
 *
 * @details
 *
 * 
 *
 * ### Notes ###
 *
 * 
 * In non macro atom mode, w_ave
 * this is an average weight (passed as w_ave), but 
 * in macro atom mode weights are never reduced (so p->w 
 * is used).
 *
 * This routine is called from bf_estimators in macro_atom modes
 * and from radiation.  Although the historical documentation
 * suggests it is only called for certain ionization modes, it appears
 * to be called in all cases, though clearly it is only provides diagnostic
 * information in some of them.
 *
 *
 * XFRAME The flux estimators are/need to be constructed in the Observer frame
 * Inputs are required in the Observer frame
 * 
 **********************************************************/



int
update_flux_estimators (xplasma, phot_mid, ds_obs, w_ave, ndom)
     PlasmaPtr xplasma;
     PhotPtr phot_mid;
     double ds_obs;
     double w_ave;
     int ndom;
{
  double flux[3];
  double p_dir_cos[3];

/* The lines below compute the flux element of this photon */

  stuff_v (phot_mid->lmn, p_dir_cos);   //Get the direction of the photon packet

  renorm (p_dir_cos, w_ave * ds_obs);   //Renormalise the direction into a flux element
  project_from_xyz_cyl (phot_mid->x, p_dir_cos, flux);  //Go from a direction cosine into a cartesian vector

  if (phot_mid->x[2] < 0)       //If the photon is in the lower hemisphere - we need to reverse the sense of the z flux
    flux[2] *= (-1);


  /*Deal with the special case of a spherical geometry */

  if (zdom[ndom].coord_type == SPHERICAL)
  {
    renorm (phot_mid->x, 1);    //Create a unit vector in the direction of the photon from the origin
    flux[0] = dot (p_dir_cos, phot_mid->x);     //In the spherical geometry, the first comonent is the radial flux
    flux[1] = flux[2] = 0.0;    //In the spherical geomerry, the theta and phi compnents are zero    
  }

/* We now update the fluxes in the three bands */


  if (phot_mid->freq < UV_low)
  {
    vadd (xplasma->F_vis, flux, xplasma->F_vis);
    xplasma->F_vis[3] += length (flux);
  }
  else if (phot_mid->freq > UV_hi)
  {
    vadd (xplasma->F_Xray, flux, xplasma->F_Xray);
    xplasma->F_Xray[3] += length (flux);
  }
  else
  {
    vadd (xplasma->F_UV, flux, xplasma->F_UV);
    xplasma->F_UV[3] += length (flux);
  }


  return (0);
}




/**********************************************************/
/**
 * @brief Updates the estimators for calculating the radiation force.
 *
 * @param[in] PlasmaPtr xplasma  PlasmaPtr for the cell of interest
 * @param[in] PhotPtr p  Photon pointer at the start of its path in the observer frame
 * @param[in] PhotPtr phot_mid  Photon pointer at the midpoint in the observer frame
 * @param[in] double ds  The path length travelled in the observer frame
 * @param[in] int ndom  The domain of interest
 * @param[in] double w_ave  The average weight along the photon's path
 * @param[in] double z  The absorbed energy fraction
 * @param[in] double frac_ff  The fractional contribution for free free opacity
 * @param[in] double frac_auger The fractional contribution to opacity from Auger ionization
 * @param[in] double frac_tot The fractional contribution
 *
 * @return Always return 0
 *
 * @details
 *
 * In non-macro mode, w_ave is an average weight but in macro mode weights are
 * never reduced, so p->w is used instead.
 *
 * ### Notes ###
 *
 * This code in this function was originally in radiation but was moved to
 * here to try and simplify radiation().
 *
 * XFRAME this function will calculate the force estimators in the observer
 * frame and requires quantities in the observer frame.
 *
 **********************************************************/


int
update_force_estimators (xplasma, p, phot_mid, ds, w_ave, ndom, z, frac_ff, frac_auger, frac_tot)
     PlasmaPtr xplasma;
     PhotPtr p, phot_mid;
     double ds, w_ave, z, frac_ff, frac_tot, frac_auger;
     int ndom;
{
  int i;
  double p_out[3], dp_cyl[3];

  /*Deal with the special case of a spherical geometry */

  if (zdom[ndom].coord_type == SPHERICAL)
  {
    renorm (phot_mid->x, 1);    //Create a unit vector in the direction of the photon from the origin
  }

  stuff_v (p->lmn, p_out);
  renorm (p_out, z * frac_ff / VLIGHT);
  if (zdom[ndom].coord_type == SPHERICAL)
  {
    dp_cyl[0] = dot (p_out, phot_mid->x);       //In the spherical geometry, the first component is the radial component
    dp_cyl[1] = dp_cyl[2] = 0.0;
  }
  else
  {
    project_from_xyz_cyl (phot_mid->x, p_out, dp_cyl);
    if (p->x[2] < 0)
      dp_cyl[2] *= (-1);
  }
  for (i = 0; i < 3; i++)
  {
    xplasma->rad_force_ff[i] += dp_cyl[i];
  }
  xplasma->rad_force_ff[3] += length (dp_cyl);

  stuff_v (p->lmn, p_out);
  renorm (p_out, (z * (frac_tot + frac_auger)) / VLIGHT);
  if (zdom[ndom].coord_type == SPHERICAL)
  {
    dp_cyl[0] = dot (p_out, phot_mid->x);       //In the spherical geometry, the first component is the radial component
    dp_cyl[1] = dp_cyl[2] = 0.0;
  }
  else
  {
    project_from_xyz_cyl (phot_mid->x, p_out, dp_cyl);
    if (p->x[2] < 0)
      dp_cyl[2] *= (-1);
  }
  for (i = 0; i < 3; i++)
  {
    xplasma->rad_force_bf[i] += dp_cyl[i];
  }

  xplasma->rad_force_bf[3] += length (dp_cyl);

  stuff_v (p->lmn, p_out);
  renorm (p_out, w_ave * ds * klein_nishina (p->freq));
  if (zdom[ndom].coord_type == SPHERICAL)
  {
    dp_cyl[0] = dot (p_out, phot_mid->x);       //In the spherical geometry, the first component is the radial component
    dp_cyl[1] = dp_cyl[2] = 0.0;
  }
  else
  {
    project_from_xyz_cyl (phot_mid->x, p_out, dp_cyl);
    if (p->x[2] < 0)
      dp_cyl[2] *= (-1);
  }
  for (i = 0; i < 3; i++)
  {
    xplasma->rad_force_es[i] += dp_cyl[i];
  }
  xplasma->rad_force_es[3] += length (dp_cyl);

  return 0;
}


/**********************************************************/
/**
 * @brief Normalise simple estimators 
 *
 * @param[in] PlasmaPtr xplasma  PlasmaPtr for the cell of interest
 *
 * @return Always return 0
 *
 * @details this routine normalises "simple" estimators. This quantities
 * vary - some of them are banded estimators used for the ionization 
 * state spectral models, others are diagnostics such as xi, others are used
 * in dilute approximations (w and t_r). The main normalisation procedure needs
 * documenting, but it normally involves dividing by a Delta V Delta t, as well
 * as (sometimes) constants like c or 4 pi. 
 *
 * ### Notes ###
 *
 * XFRAME this function will calculate the force estimators in the observer
 * frame and requires quantities in the observer frame.
 *
 **********************************************************/

int
normalise_simple_estimators (xplasma)
     PlasmaPtr xplasma;
{
  int i, nwind;
  double radiation_temperature, nh, wtest;
  double volume_cmf, volume_obs;
  double electron_density_obs;

  nwind = xplasma->nwind;

  /* XFRAME -- this needs to be the correct volume, or more correctly, the correct Delta V Delta t */
  volume_cmf = wmain[nwind].vol;
  volume_obs = volume_cmf / wmain[nwind].xgamma_cen;
  electron_density_obs = xplasma->ne / wmain[nwind].xgamma_cen; // Mihalas & Mihalas p146

  if (xplasma->ntot > 0)
  {
    wtest = xplasma->ave_freq;
    xplasma->ave_freq /= xplasma->j;    /* Normalization to frequency moment */
    if (sane_check (xplasma->ave_freq))
    {
      Error ("normalise_simple_estimators:sane_check %d ave_freq %e j %e ntot %d\n", xplasma->nplasma, wtest, xplasma->j, xplasma->ntot);
    }

    xplasma->j /= (4. * PI * volume_cmf);
    xplasma->j_direct /= (4. * PI * volume_cmf);
    xplasma->j_scatt /= (4. * PI * volume_cmf);

    xplasma->t_r_old = xplasma->t_r;    // Store the previous t_r in t_r_old immediately before recalculating
    radiation_temperature = xplasma->t_r = PLANCK * xplasma->ave_freq / (BOLTZMANN * 3.832);
    xplasma->w =
      PI * xplasma->j / (STEFAN_BOLTZMANN * radiation_temperature * radiation_temperature * radiation_temperature * radiation_temperature);

    if (xplasma->w > 1e10)
    {
      Error ("normalise_simple_estimators: Huge w %8.2e in cell %d trad %10.2e j %8.2e\n", xplasma->w, xplasma->nplasma,
             radiation_temperature, xplasma->j);
    }
    if (sane_check (radiation_temperature) || sane_check (xplasma->w))
    {
      Error ("normalise_simple_estimators:sane_check %d trad %8.2e w %8.2g\n", xplasma->nplasma, radiation_temperature, xplasma->w);
      Error ("normalise_simple_estimators: ave_freq %8.2e j %8.2e\n", xplasma->ave_freq, xplasma->j);
      Exit (0);
    }
  }
  else
  {                             /* It is not clear what to do with no photons in a cell */
    xplasma->j = xplasma->j_direct = xplasma->j_scatt = 0;
    xplasma->t_e *= 0.7;
    if (xplasma->t_e < MIN_TEMP)
      xplasma->t_e = MIN_TEMP;
    xplasma->w = 0;
  }

  /* Calculate the frequency banded j and ave_freq variables */

  for (i = 0; i < geo.nxfreq; i++)
  {                             /*loop over number of bands */
    if (xplasma->nxtot[i] > 0)
    {                           /*Check we actually have some photons in the cell in this band */
      xplasma->xave_freq[i] /= xplasma->xj[i];  /*Normalise the average frequency */
      xplasma->xsd_freq[i] /= xplasma->xj[i];   /*Normalise the mean square frequency */
      xplasma->xsd_freq[i] = sqrt (xplasma->xsd_freq[i] - (xplasma->xave_freq[i] * xplasma->xave_freq[i]));     /*Compute standard deviation */
      xplasma->xj[i] /= (4 * PI * volume_cmf);  /*Convert to radiation density */
    }
    else
    {
      xplasma->xj[i] = 0;       /*If no photons, set both radiation estimators to zero */
      xplasma->xave_freq[i] = 0;
      xplasma->xsd_freq[i] = 0; /*NSH 120815 and also the SD ???? */
    }
  }

  /* End of loop */

  nh = xplasma->rho * rho2nh;

  /* 1110 NSH Normalise IP, which at this point should be
   * the number of photons in a cell by dividing by volume
   * and number density of hydrogen in the cell
   */

  xplasma->ip /= (VLIGHT * volume_cmf * nh);
  xplasma->ip_direct /= (VLIGHT * volume_cmf * nh);
  xplasma->ip_scatt /= (VLIGHT * volume_cmf * nh);

  /* 1510 NSH Normalise xi, which at this point should be the luminosity of
   * ionizing photons in a cell (just the sum of photon weights)
   */

  xplasma->xi *= 4. * PI;
  xplasma->xi /= (volume_cmf * nh);

  /*
   * XFRAME -- the radiation force and flux estimators are all observer frame
   * quantities and hence need to be normalised with observer frame volumes
   * and densities
   */

  for (i = 0; i < 4; i++)
  {
    xplasma->rad_force_es[i] *= (volume_obs * electron_density_obs) / (volume_obs * VLIGHT);
    xplasma->F_vis[i] /= volume_obs;
    xplasma->F_UV[i] /= volume_obs;
    xplasma->F_Xray[i] /= volume_obs;
  }

  return (0);
}