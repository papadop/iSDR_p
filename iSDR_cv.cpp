#include <cxxstd/iostream.h>
#include <flens/flens.cxx>
#include "matio.h"
#include <cmath>
#include <vector>
#include "iSDR.h"
#include <stdlib.h>
#include "ReadWriteMat.h"
#include <omp.h>
using namespace flens;
using namespace std;
#include <random>   // for default_random_engine & uniform_int_distribution<int>
#include <chrono>   // to provide seed to the default_random_engine
using namespace std;

default_random_engine dre (chrono::steady_clock::now().time_since_epoch().count());     // provide seed
int RANDOM (int lim)
{
    uniform_int_distribution<int> uid {0,lim};   // help dre to generate nos from 0 to lim (lim included);
    return uid(dre);    // pass dre as an argument to uid to generate the random no
}

int WriteData(const char *file_path, double *alpha, double *cv_fit_data,
                double alpha_max,int n_alpha, int n_Kfold){
    /* This function write the results of the K-fold cross-validation into a
     * mat file.
     *      file_path: file name and location to where you wanna write results
     *      alpha: a 1D array (n_alpha) containing the alpha values used in the 
     *             crossvalidation
     *      cv_fit_data: 2D array (n_Kfoldxn_alpha) containing the fit error of
     *                   each alpha with different runs
     *      alpha_max: maximum value of alpha in which all sources/regions are
     *                 inactive
     *      n_alpha: length of 1D array "alpha"
     *      n_Kfold: number of runs for each alpha value.
     * */
    double vec1[n_alpha];
    double mat1[n_alpha][n_Kfold];
    double sca1[1];
    int j,i;
    for(j=0;j<n_alpha;j++){
      vec1[j] = alpha[j];
    }
    for(j=0;j<n_alpha;j++){
        for(i=0;i<n_Kfold;i++)
            mat1[j][i] = cv_fit_data[n_Kfold*j+i];
    }
    sca1[0] = alpha_max;
    /* setup the output */
    mat_t *mat;
    matvar_t *matvar;
    size_t dim1d[1] = {(unsigned int)n_alpha};
    size_t dim2d[1] = {(unsigned int)1};
    size_t dims2[2] = {(unsigned int)n_Kfold, (unsigned int)n_alpha};
    mat = Mat_Open(file_path, MAT_ACC_RDWR);
    mat = Mat_Create(file_path,NULL);
    if(mat){
        matvar = Mat_VarCreate("Alpha",MAT_C_DOUBLE,MAT_T_DOUBLE,1, dim1d,
        &vec1,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        matvar = Mat_VarCreate("CV data fit", MAT_C_DOUBLE, MAT_T_DOUBLE,2,
        dims2, &mat1,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        matvar = Mat_VarCreate("Alpha max",MAT_C_DOUBLE,MAT_T_DOUBLE,1, dim2d,
        &sca1,0);
        Mat_VarWrite( mat, matvar, MAT_COMPRESSION_NONE);
        Mat_VarFree(matvar);
        Mat_Close(mat);
    }
    else{
        printf("Failed to save results in %s", file_path);
        return 1;
    }
    return 0;
}

void explain_para(){
    printf( " ./iSDR  arg1 arg2 arg3 arg4 arg5 arg6 arg7 arg8\n");
    printf( "      arg1 : path to mat file that contains MEG/EEG, G, GA\n");
    printf( "      arg2 : min value of regularization parameter >0. \n");
    printf( "      arg3 : max value of regularization parameter <100. \n");
    printf( "      arg4 : N of reg parameters in the range [alpha_min, alpha_max]. \n");
    printf( "      arg5 : N of KFold. \n");
    printf( "      arg6 : N of repetions of the KFold for each alpha_i. \n");
    printf( "      arg7 : where to save results. \n");
    printf( "      arg8 : verbose. \n");
}

void printHelp(){
    printf("\n--help or -h of the iterative source and dynamics reconstruction algorithm.\n");
    printf(" This code uses KFold cross-validation technique to choose the regularization parameter\n");
    printf(" of iSDR. The actual version of the code needs 9 inputs:\n");
    printf(" The output of this function is a mat file that contains the following:\n");
    printf("    Alpha: vector (arg4) contains the alpha's used in the kFold\n");
    printf("    CV data fit: a matrix (arg5xarg4) which contains the crossvalidation fit error value\n");
    printf("    Alpha max: a scaler. the smallest alpha that gives an empty active set\n");
    explain_para();
}

void print_param(int n_s, int n_t, int n_c, int m_p, double alpha,
                 double d_w_tol){
    printf(" N of sensors %d\n", n_c);
    printf(" N of sources %d\n", n_s);
    printf(" N of samples %d\n", n_t);
    printf(" MAR model    %d\n", m_p);
    printf(" iSDR tol   %.3e\n", d_w_tol);
    printf(" iSDR (p : =  %d with alpha : = %.2f%%\n", m_p, alpha);
}

double CV_error_magbias(double * GA, double *M, int n_s, int n_c, int n_t, int m_p){
    int n_t_s = n_t + m_p -1;
    typedef GeMatrix<FullStorage<double> >   GeMatrix;
    typedef typename GeMatrix::IndexType     IndexType;
    typedef DenseVector<Array<double> >      DenseVectord;
    const Underscore<IndexType>  _;
    GeMatrix BigG(n_c*n_t, n_s*n_t_s);
    GeMatrix BigG2(n_c*n_t, n_s*n_t_s);
    int s_max = std::max(n_c*n_t, n_s*n_t_s);
    DenseVectord y(s_max);
    for (int i=0; i<n_t; i++){
        int shift_x = i*n_c+1;
        int shift_y = n_s*i+1;
        for (int j=0;j<n_c;j++){
            for(int k=0;k<n_s*m_p;k++){
                BigG(shift_x+j, shift_y+k) = GA[k*n_c+j];
                BigG2(shift_x+j, shift_y+k) = GA[k*n_c+j];
            }
            y(1+i*n_c+j) = M[i*n_c+j];
        }
    }
    lapack::ls(NoTrans, BigG2, y);
    DenseVectord  M_rec(n_c*n_t);
    DenseVectord  S(n_s*n_t_s);
    for (int i=1;i<=n_t_s*n_s;i++)
        S(i) = y(i);
    cxxblas::gemv(BigG.order(), NoTrans, BigG.numRows(), BigG.numCols(),
                  1.0, BigG.data(), BigG.leadingDimension(), S.data(),
                  S.stride(), 0.0, M_rec.data(), M_rec.stride());
    cxxblas::axpy(n_t*n_c,-1.0, &M[0], 1, &M_rec.data()[0], 1);
    double cv_k;
    cxxblas::nrm2(n_t*n_c, &M_rec.data()[0], 1, cv_k);
    return cv_k*cv_k/n_c;
}
int main(int argc, char* argv[]){
    std::string str1 ("-h");
    std::string str2 ("--help");
    if (str1.compare(argv[1]) == 0 || str2.compare(argv[1]) == 0){
        printHelp();
        return 1;
    }
    if(argc < 9){
        printf("Missing arguments:\n");
        explain_para();
        return 1;
    }
    else{
        bool verbose = false;
        if (atoi(argv[8]) == 1)
            verbose = true; 
        int n_c = 306;int n_s = 600;int m_p = 3;int n_t = 297;
        int n_iter_mxne = 10000;int n_iter_iSDR = 100;
        const char *file_path = argv[1];
        double alpha_min = atof(argv[2]);
        double alpha_max_ = atof(argv[3]);
        int n_alpha = atoi(argv[4]);
        int Kfold = atoi(argv[5]);
        int n_Kfold = atoi(argv[6]);
        const char *save_path = argv[7];
        double d_w_tol=1e-7;
        int re_use = 1;
        int n_t_s = n_t + m_p - 1;
        ReadWriteMat _RWMat(n_s, n_c, m_p, n_t);
        _RWMat.Read_parameters(file_path);
        n_s = _RWMat.n_s;
        n_c = _RWMat.n_c;
        m_p = _RWMat.m_p;
        n_t = _RWMat.n_t;
        n_t_s = _RWMat.n_t_s;
        int block = n_c / Kfold;
        if (verbose){
            printf("%d values of alpha in [%.2f, %.2f], \n", n_alpha, 
            alpha_min, alpha_max_);
            printf("KFold %02d \n", Kfold);
            printf("Input file %s \n", file_path);
            printf("Output file %s \n", save_path);
            printf("Block size %d \n", block);
        }
        double *G_o = new double [n_c*n_s];
        double *GA_initial = new double [n_c*n_s*m_p];
        double *M = new double [n_c*n_t];
        int *SC = new int [n_s*n_s];
        bool use_mxne = false;
        if (re_use==1)
            use_mxne = true;
        double *Acoef= new double [n_s*n_s*m_p];
        int *Active= new int [n_s];
        _RWMat.ReadData(file_path, G_o, GA_initial, M, SC);
        double mvar_th = 1e-3;
        double *ALPHA = new double[n_alpha];
        double alp_step = (alpha_max_ - alpha_min)/(float)n_alpha;
        for (int y=0; y< n_alpha;y++){
            ALPHA[y] = alpha_min + y*alp_step;
        }
        double * GA_reorder = new double [n_c*n_s*m_p];
        double * G_reorder_ptr = &GA_reorder[0];
        iSDR _iSDR(n_s, n_c, m_p, n_t, 1.0, n_iter_mxne, n_iter_iSDR,
                    d_w_tol, mvar_th, verbose);
        _iSDR.Reorder_G(&GA_initial[0], G_reorder_ptr);// reorder gain matrix
        MxNE _MxNE(n_s, n_c, m_p, n_t, d_w_tol, verbose);
        double alpha_max = _MxNE.Compute_alpha_max(&G_reorder_ptr[0], M);
        double * cv_fit_data = new double [n_alpha*n_Kfold];
        double * alpha_real = new double[n_alpha];
        for (int x=0;x<n_alpha;x++)
            alpha_real[x] = 0.01*alpha_max*ALPHA[x];
        int n_cpu = omp_get_num_procs();
        printf("OMP uses %d cpus \n", n_cpu);
        int x, r_s;
        double m_norm;
        cxxblas::nrm2(n_t*n_c, &M[0], 1, m_norm);
        m_norm *= m_norm/n_c;
        #pragma omp parallel for default(shared) private(r_s, x) collapse(2) num_threads(n_cpu)
        for (r_s = 0; r_s<n_Kfold; r_s++){
            for (x = 0; x<n_alpha ;x++){
                double alpha = alpha_real[x];
                std::vector<int> sensor_list;
                std::vector<int> sensor_all;
                for (int y=0; y< n_c;y++){
                    sensor_list.push_back(y);
                    sensor_all.push_back(y);
                }
                double error_cv_alp = 0.0;
                for (int i=0; i<Kfold; i++){
                    double *J = new double [n_s*n_t_s];
                    std::fill(&J[0], &J[n_t_s*n_s], 0.0);
                    std::vector<int> sensor_kfold;
                    int set = block;
                    if (i == Kfold-1)
                        set = n_c - (Kfold-1)*block;
                    for (int j=0;j<set;j++){
                        int n_c_i = RANDOM(sensor_list.size()-1);//std::rand() % sensor_list.size();
                        sensor_kfold.push_back(sensor_list[n_c_i]);
                        sensor_list.erase(sensor_list.begin() + n_c_i);
                    }
                    std::sort(sensor_kfold.begin(), sensor_kfold.end());
                    int set_i = n_c - set;
                    double * Mn = new double [set_i * n_t];
                    double * G_on = new double[set_i * n_s];
                    double * GA_n = new double[set_i*n_s*m_p];
                    std::fill(&Mn[0], &Mn[set_i * n_t], 0.0);
                    std::fill(&G_on[0], &G_on[set_i * n_s], 0.0);
                    std::fill(&GA_n[0], &GA_n[set_i * n_s * m_p], 0.0);
                    int z = 0;
                    for (int j =0; j<n_c; j++){
                        bool te = false;
                        for(int k=0;k<set;k++){
                            if (j == sensor_kfold[k]){
                                te = true;
                                break;
                            }
                        }
                        if (not te){
                            cxxblas::copy(n_t, &M[j], n_c, &Mn[z], set_i);
                            cxxblas::copy(n_s*m_p, &GA_initial[j], n_c,
                            &GA_n[z], set_i);
                            cxxblas::copy(n_s, &G_o[j], n_c, &G_on[z], set_i);
                            z += 1;
                        }
                    }
                    if (z != set_i){
                        printf("Error in the %02d th Kfold", i);
                        std::cout<<z<<" "<<set_i<<std::endl;
                        break;}
                    iSDR _iSDR_(n_s, set_i, m_p, n_t, alpha, n_iter_mxne,
                    n_iter_iSDR, d_w_tol, mvar_th, false);
                    int n_s_e = _iSDR_.iSDR_solve(&G_on[0], SC, &Mn[0],
                    &GA_n[0], &J[0], &Acoef[0], &Active[0], use_mxne, true);
                    double * Mtmp = new double [set*n_t];
                    std::fill(&Mtmp[0], &Mtmp[set*n_t], 0.0);
                    if (n_s_e > 0){
                        double * Gx = new double [n_s_e*set];
                        for (int t =0;t<n_s_e;t++)
                            cxxblas::copy(set, &G_o[sensor_kfold[t]*n_c], 1,
                            &Gx[set*t], 1);
                        double * GA_es = new double[set*n_s_e*m_p];
                        cxxblas::gemm(cxxblas::ColMajor,cxxblas::NoTrans,
                        cxxblas::NoTrans, set, n_s_e*m_p, n_s_e, 1.0, &Gx[0],
                        set, &Acoef[0], n_s_e, 0.0, &GA_es[0], set);

                        /*for (int p =0;p<m_p;p++)
                            for(int ji=0;ji<n_t; ++ji)
                                for (int k =0;k<set;k++)
                                    for(int ii=0;ii<n_s_e; ii++){
                                        double q = J[ii*n_t_s + ji + p];
                                        double w = GA_es[(p*n_s_e+ii)*set + k];
                                        Mtmp[ji*set + k] +=  q * w;
                                    }*/
                        delete[] Gx;
                        double * Mcomp = new double [set*n_t];
                        std::fill(&Mcomp[0], &Mcomp[set*n_t], 0.0);
                        for (int k =0;k<set;k++)
                            cxxblas::copy(n_t, &M[sensor_kfold[k]], n_c, &Mcomp[k],
                            set);
                        double cv_k = CV_error_magbias(&GA_es[0], &Mcomp[0], n_s_e, set, n_t, m_p);
                        //cxxblas::axpy(n_t*set,-1.0, &Mcomp[0], 1, &Mtmp[0], 1);
                        //double cv_k;
                        //cxxblas::nrm2(n_t*set, &Mtmp[0], 1, cv_k);
                        error_cv_alp += cv_k;
                        delete[] GA_es;
                        delete[] Mcomp;
                    }
                    else
                        error_cv_alp += m_norm;

                    delete[] G_on;
                    delete[] GA_n;
                    delete[] Mn;
                    delete[] J;
                    delete[] Mtmp;
                }
                error_cv_alp /= Kfold;
                int in_dex = r_s+x*n_Kfold;
                cv_fit_data[in_dex] = error_cv_alp;
                if (verbose){
                    //printf("run %.03f %% \n", percentage);
                    //std::cerr<<"alpha["<<x<<"] = "<< alpha/alpha_max*100.0<<" | data_fit["<<r_s<<"] = "<<error_cv_alp <<std::endl;
                    printf("alpha[%03d] = %.2f %% | data_fit[%03d] = %.6e \n", x, alpha/alpha_max*100.0, r_s, error_cv_alp);
                }
            }
            //if (verbose)
            //printf("run %03d %% \n", percentage);
        }
        delete[] G_o;
        delete[] GA_initial;
        delete[] M;
        delete[] SC;
        delete[] Acoef;
        delete[] Active;
        delete[] GA_reorder;
        delete[] ALPHA;
        WriteData(save_path, &alpha_real[0], &cv_fit_data[0], alpha_max, n_alpha, n_Kfold);
        delete[] cv_fit_data;
        delete[] alpha_real;
    }
    return 0;
}
