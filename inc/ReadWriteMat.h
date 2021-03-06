#pragma once

#include <cxxstd/iostream.h>
#include <flens/flens.cxx>
#include "matio.h"
#include <cmath>
#include <ctime>
#include <algorithm>
#include <omp.h>
#include <string>
#include <vector>
//==============================================================================
//==============================================================================
///
/// \file ReadWriteMat.h
///
/// \author Brahim Belaoucha, INRIA <br>
///         Copyright (c) 2017  <br>
//==============================================================================
//==============================================================================

class ReadWriteMat {
private:

        
public:
        long unsigned int n_t;
        long unsigned int n_c;
        long unsigned int n_t_s;
        long unsigned int m_p;
        long unsigned int n_s;
        ReadWriteMat(int n_sources, int n_sensors, int Mar_model, int n_samples);
        ~ReadWriteMat(){};
        void ReadData(const char *file_path, double *G_o, double *G, double *R, 
                       int * SC) const;
        int WriteData(const char *file_path, double *S, double *mvar, int *A, 
                    double * w);
        void Read_parameters(const char *file_path);
};
 
