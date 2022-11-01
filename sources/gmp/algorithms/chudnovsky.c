#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <omp.h>
#include "mpi.h"
#include "../mpi_operations.h"

#define A 13591409
#define B 545140134
#define C 640320
#define D 426880
#define E 10005

/************************************************************************************
 * Miguel Pardo Navarro. 17/07/2021                                                 *
 * Chudnovsky formula implementation                                                *
 * This version does not computes all the factorials                                *
 * This version allows computing Pi using processes and threads in hybrid way.      *
 *                                                                                  *
 ************************************************************************************
 * Chudnovsky formula:                                                              *
 *     426880 sqrt(10005)                 (6n)! (545140134n + 13591409)             *
 *    --------------------  = SUMMATORY( ----------------------------- ),  n >=0    *
 *            pi                            (n!)^3 (3n)! (-640320)^3n               *
 *                                                                                  *
 * Some operands of the formula are coded as:                                       *
 *      dep_a_dividend = (6n)!                                                      *
 *      dep_a_divisor  = (n!)^3 (3n)!                                               *
 *      e              = 426880 sqrt(10005)                                         *
 *                                                                                  *
 ************************************************************************************
 * Chudnovsky formula dependencies:                                                 *
 *                     (6n)!         (12n + 10)(12n + 6)(12n + 2)                   *
 *      dep_a(n) = --------------- = ---------------------------- * dep_a(n-1)      *
 *                 ((n!)^3 (3n)!)              (n + 1)^3                            *
 *                                                                                  *
 *      dep_b(n) = (-640320)^3n = (-640320)^3(n-1) * (-640320)^3)                   *
 *                                                                                  *
 *      dep_c(n) = (545140134n + 13591409) = dep_c(n - 1) + 545140134               *
 *                                                                                  *
 ************************************************************************************/


/*
 * An iteration of Chudnovsky formula
 */
void chudnovsky_iteration_gmp(mpf_t pi, int n, mpf_t dep_a, mpf_t dep_b, mpf_t dep_c, mpf_t aux){
    mpf_mul(aux, dep_a, dep_c);
    mpf_div(aux, aux, dep_b);
    
    mpf_add(pi, pi, aux);
}


/*
 * This method is used by ParallelChudnovskyAlgorithm procs
 * for computing the first value of dep_a
 */
void init_dep_a_gmp(mpf_t dep_a, int block_start){
    mpz_t factorial_n, dividend, divisor;
    mpf_t float_dividend, float_divisor;
    mpz_inits(factorial_n, dividend, divisor, NULL);
    mpf_inits(float_dividend, float_divisor, NULL);

    mpz_fac_ui(factorial_n, block_start);
    mpz_fac_ui(divisor, 3 * block_start);
    mpz_fac_ui(dividend, 6 * block_start);

    mpz_pow_ui(factorial_n, factorial_n, 3);
    mpz_mul(divisor, divisor, factorial_n);

    mpf_set_z(float_dividend, dividend);
    mpf_set_z(float_divisor, divisor);

    mpf_div(dep_a, float_dividend, float_divisor);

    mpz_clears(factorial_n, dividend, divisor, NULL);
    mpf_clears(float_dividend, float_divisor, NULL);
}

/*
 * Parallel Pi number calculation using the Chudnovsky algorithm
 * The number of iterations is divided by blocks 
 * so each process calculates a part of pi with multiple threads (or just one thread). 
 * Each process will split the iterations cyclically
 * among the threads to calculate its part.  
 * Finally, a collective reduction operation will be performed 
 * using a user defined function in OperationsMPI. 
 */
void chudnovsky_algorithm_gmp(int num_procs, int proc_id, mpf_t pi, int num_iterations, int num_threads){
    int packet_size, position, block_size, block_start, block_end; 
    mpf_t local_proc_pi, e, c;  

    block_size = (num_iterations + num_procs - 1) / num_procs;
    block_start = proc_id * block_size;
    block_end = block_start + block_size;
    if (block_end > num_iterations) block_end = num_iterations;

    mpf_init_set_ui(local_proc_pi, 0);   
    mpf_init_set_ui(e, E);
    mpf_init_set_ui(c, C);
    mpf_neg(c, c);
    mpf_pow_ui(c, c, 3 * num_threads);

    //Set the number of threads 
    omp_set_num_threads(num_threads);

    #pragma omp parallel 
    {
        int thread_id, i, factor_a;
        mpf_t local_thread_pi, dep_a, dep_a_dividend, dep_a_divisor, dep_b, dep_c, aux;

        thread_id = omp_get_thread_num();
       
        mpf_init_set_ui(local_thread_pi, 0);    // private thread pi
        mpf_inits(dep_a, dep_b, dep_a_dividend, dep_a_divisor, aux, NULL);
        init_dep_a_gmp(dep_a, block_start + thread_id);
        mpf_pow_ui(dep_b, c, block_start + thread_id);
        mpf_init_set_ui(dep_c, B);
        mpf_mul_ui(dep_c, dep_c, block_start + thread_id);
        mpf_add_ui(dep_c, dep_c, A);
        factor_a = 12 * (block_start + thread_id);

        //First Phase -> Working on a local variable        
        #pragma omp parallel for 
            for(i = block_start + thread_id; i < block_end; i += num_threads){
                if (thread_id == 0) {
                    printf("#################### iteration: %i \n", i);
                    gmp_printf("dep_a: %.Ff \n", dep_a);
                    gmp_printf("dep_b: %.Ff \n", dep_b);
                    gmp_printf("dep_c: %.Ff \n", dep_c);
                    // El problema está en la primera iteración de las hebras distintas de cero ya que para ellas dep_a(i - 1) = 1  ??
                } 
                chudnovsky_iteration_gmp(local_thread_pi, i, dep_a, dep_b, dep_c, aux);
                //Update dep_a:

                factor_a = 12 * i; 
                mpf_set_ui(dep_a_dividend, factor_a + 10);
                mpf_mul_ui(dep_a_dividend, dep_a_dividend, factor_a + 6);
                mpf_mul_ui(dep_a_dividend, dep_a_dividend, factor_a + 2);
                mpf_mul(dep_a_dividend, dep_a_dividend, dep_a);

                mpf_set_ui(dep_a_divisor, i + 1);
                mpf_pow_ui(dep_a_divisor, dep_a_divisor, 3);
                mpf_div(dep_a, dep_a_dividend, dep_a_divisor);

                //Update dep_b:
                mpf_mul(dep_b, dep_b, c);

                //Update dep_c:
                mpf_add_ui(dep_c, dep_c, B * num_threads);
            }

        //Second Phase -> Accumulate the result in the global variable 
        #pragma omp critical
        mpf_add(local_proc_pi, local_proc_pi, local_thread_pi);

        //Clear thread memory
        mpf_clears(local_thread_pi, dep_a, dep_a_dividend, dep_a_divisor, dep_b, dep_c, aux, NULL);   
    }
    
    //Create user defined operation
    MPI_Op add_op;
    MPI_Op_create((MPI_User_function *)add_gmp, 0, &add_op);

    //Set buffers for cumunications and position for pack and unpack information 
    packet_size = 8 + sizeof(mp_exp_t) + ((local_proc_pi -> _mp_prec + 1) * sizeof(mp_limb_t));
    char recbuffer[packet_size];
    char sendbuffer[packet_size];

    //Pack local_proc_pi in sendbuffuer
    position = pack_gmp(sendbuffer, local_proc_pi);

    //Reduce local_proc_pi
    MPI_Reduce(sendbuffer, recbuffer, position, MPI_PACKED, add_op, 0, MPI_COMM_WORLD);

    //Unpack recbuffer in global Pi and do the last operations to get Pi
    if (proc_id == 0){
        unpack_gmp(recbuffer, pi);
        mpf_sqrt(e, e);
        mpf_mul_ui(e, e, D);
        mpf_div(pi, e, pi); 
    }    

    //Clear process memory
    MPI_Op_free(&add_op);
    mpf_clears(local_proc_pi, e, c, NULL);
}

