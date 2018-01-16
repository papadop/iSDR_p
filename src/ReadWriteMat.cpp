#include "ReadWriteMat.h"
#include <omp.h>
#include "Matrix.h"

ReadWriteMat::ReadWriteMat(int n_sources, int n_sensors, int Mar_model,
    int n_samples){
    this-> n_t = n_samples;
    this-> n_c = n_sensors;
    this-> n_s = n_sources;
    this-> m_p = Mar_model;
    this-> n_t_s = n_t + m_p - 1;
}

int ReadWriteMat::Read_parameters(const char *file_path){
    mat_t *matfp; // use matio to read the .mat file
    matvar_t *matvar;
    matfp = Mat_Open(file_path, MAT_ACC_RDONLY);
    if (matfp != NULL){
        matvar = Mat_VarRead(matfp, "n_s") ;
        const int *x = static_cast<const int*>(matvar->data);
        n_s = x[0];
        matvar = Mat_VarRead(matfp, "n_c") ;
        x = static_cast<const int*>(matvar->data);
        n_c = x[0];
        matvar = Mat_VarRead(matfp, "n_t") ;
        x = static_cast<const int*>(matvar->data);
        n_t = x[0];
        matvar = Mat_VarRead(matfp, "m_p") ;
        x = static_cast<const int*>(matvar->data);
        m_p = x[0];
        n_t_s = n_t + m_p - 1;
    }
    else{
        std::cerr<<file_path<<"  is not found"<<std::endl;
        return 1;
    }
    return 0;
}
int ReadWriteMat::ReadData(const char *file_path, Maths::DMatrix &G_o,
    Maths::DMatrix &GA, Maths::DMatrix &R, Maths::IMatrix &SC) const{
    // read data from mat file
    mat_t *matfp; // use matio to read the .mat file
    matvar_t *matvar;
    matfp = Mat_Open(file_path, MAT_ACC_RDONLY);
    if (matfp != NULL){
        matvar = Mat_VarRead(matfp, "GA") ;
        const double *xData = static_cast<const double*>(matvar->data);
        for(long unsigned int y = 0;y < n_s*m_p; ++y){
            for (long unsigned int x = 0; x < n_c; ++x)
                GA.data()[x + y*n_c] = xData[x + y*n_c]; 
        }
        Mat_VarFree(matvar);
        matvar = Mat_VarRead(matfp, "M") ;
        const double *xData1 = static_cast<const double*>(matvar->data);
        for(long unsigned int y = 0;y < n_t; ++y){
            for (long unsigned int x = 0; x < n_c; ++x)
                R.data()[x + y*n_c] = xData1[x+y*n_c];
        }
        Mat_VarFree(matvar);
        matvar = Mat_VarRead(matfp, "G") ;
        const double *xData_ = static_cast<const double*>(matvar->data);
        for(long unsigned int y = 0;y < n_s; ++y){
            for (long unsigned int x = 0; x < n_c; ++x)
                G_o.data()[x + y*n_c] = xData_[x+y*n_c];
        }
        Mat_VarFree(matvar);
        matvar = Mat_VarRead(matfp, "SC") ;
        const double *xData2 = static_cast<const double*>(matvar->data);
        //#pragma omp parallel for
        for(long unsigned int y = 0;y < n_s; y++){
            for (long unsigned int x = 0; x < n_s; x++)
                SC.data()[x*n_s + y] = (int)xData2[x*n_s+y];
        }
        xData_ = NULL;
        xData = NULL;
        xData1 = NULL;
        xData2 = NULL;
        Mat_VarFree(matvar);
        Mat_Close(matfp);
    }
    else{
        std::cerr<<file_path<<" is not found"<<std::endl;
        return 1;
    }
    return 0;
}


int ReadWriteMat::WriteData(const char *file_path, Maths::DMatrix &S, Maths::DMatrix &mvar,
                Maths::DMatrix &mvar_n, Maths::IVector &A, Maths::DVector &w, double max_eigenvalue){
    double mat1[n_s][n_t_s];
    double mat2[n_s*m_p][n_s];
    double mat2_[n_s*m_p][n_s];
    double mat3[n_s];
    double mat4[n_s];
    unsigned int i, j;
    for(j=0;j<n_s;j++){
        for(i=0;i<n_t_s;i++)
            mat1[j][i] = S.data()[n_t_s*j+i];
    }
    for(i=0;i<n_s * m_p;i++){
        for(j=0;j<n_s;j++){
            mat2[i][j] = mvar.data()[j + n_s*i];
            mat2_[i][j] = mvar_n.data()[j + n_s*i];
        }
    }
    for(j=0;j<n_s;j++){
      mat3[j] = A.data()[j];
      mat4[j] = w.data()[j];
    }
    /* setup the output */
    mat_t *mat;
    matvar_t *matvar;
    size_t dims1[2] = {n_t_s,n_s};
    size_t dims2[2] = {n_s,n_s*m_p};
    size_t dim1d[1] = {n_s};
    double sca1[1];
    sca1[0] = max_eigenvalue;
    size_t dims[1] = {(unsigned int)1};
    mat = Mat_Create(file_path,NULL);
    if(mat != NULL){
        matvar = Mat_VarCreate("Eigen max", MAT_C_DOUBLE, MAT_T_DOUBLE,1, dims, &sca1,0);
        Mat_VarWrite(mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        /* Estimated brain activation */
        matvar = Mat_VarCreate("S estimate",MAT_C_DOUBLE,MAT_T_DOUBLE,2, dims1, &mat1,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        /* multivariate autoregresive model elements */
        matvar = Mat_VarCreate("MVAR", MAT_C_DOUBLE,MAT_T_DOUBLE,2, dims2, &mat2,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        matvar = Mat_VarCreate("MVAR normalized", MAT_C_DOUBLE,MAT_T_DOUBLE,2, dims2, &mat2_,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        /* Label of active sources/regions */
        matvar = Mat_VarCreate("Active set",MAT_C_DOUBLE,MAT_T_DOUBLE,1, dim1d, &mat3,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        /* weights used to normalize MVAR coeffitions */
        matvar = Mat_VarCreate("Weights", MAT_C_DOUBLE, MAT_T_DOUBLE,1, dim1d, &mat4,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        Mat_Close(mat);
    }
    else{
        printf("Failed to save results in %s", file_path);
        printf("check if you have the righ to write there");
        return 1;
    }
    return 0;
}
