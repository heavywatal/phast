#ifndef DGAMMA_H
#define DGAMMA_H

#include <stdlib.h>

/* inverse CDF Gamma (quantile function); see Yang 1994, p. 308
   (discrete Gamma paper) */
#define PointGamma(prob,alpha,beta) PointChi2(prob,2.0*(alpha))/(2.0*(beta))

/* inverse CDF chi sq (quantile function) */
double PointChi2 (double prob, double v);

/* these for use by PointChi2 */
double PointNormal (double prob);
double LnGamma (double x);

/* Incomplete gamma ratio, otherwise known as complementary normalized
   incomplete gamma function */
double IncompleteGamma (double x, double alpha, double ln_gamma_alpha);

int DiscreteGamma (double freqK[], double rK[], 
                   double alpha, double beta, int K, int median);

#endif