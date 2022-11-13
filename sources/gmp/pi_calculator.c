#include <stdio.h>
#include <stdlib.h>
#include <gmp.h>
#include <time.h>
#include <stdbool.h>
#include "mpi.h"
#include "algorithms/bbp_blocks_and_cyclic.h"
#include "algorithms/bellard_blocks_and_cyclic.h"
#include "algorithms/chudnovsky_blocks_and_blocks.h"
#include "algorithms/chudnovsky_blocks_and_cyclic.h"
#include "check_decimals.h"
#include "../common/printer.h"


double gettimeofday();


void check_errors_gmp(int num_procs, int precision, int num_iterations, int num_threads, int proc_id, int algorithm){
    if (precision <= 0){
        if(proc_id == 0) printf("  Precision should be greater than cero. \n\n");
        MPI_Finalize();
        exit(-1);
    } 
    if (num_iterations < (num_threads * num_procs)){
        if(proc_id == 0){
            printf("  The number of iterations required for the computation is too small to be solved with %d threads and %d procesess. \n", num_threads, num_procs);
            printf("  Try using a greater precision or lower threads/processes number. \n\n");
        }
        MPI_Finalize();
        exit(-1);
    }
}


void calculate_pi_gmp(int num_procs, int proc_id, int algorithm, int precision, int num_threads, bool print_in_csv_format){
    double execution_time;
    struct timeval t1, t2;
    int num_iterations, decimals_computed; 
    mpf_t pi;
    char *algorithm_type;


    //Get init time 
    if(proc_id == 0){
        gettimeofday(&t1, NULL);
    }

    //Set gmp float precision (in bits) and init pi
    mpf_set_default_prec(precision * 8); 
    if (proc_id == 0){
        mpf_init_set_ui(pi, 0);
    }
    
    
    switch (algorithm)
    {
    case 0:
        num_iterations = precision * 0.84;
        check_errors_gmp(num_procs, precision, num_iterations, num_threads, proc_id, algorithm);
        algorithm_type = "BBP (Processes distributes the iterations in blocks and threads do it cyclically)";
        bbp_blocks_and_cyclic_algorithm_gmp(num_procs, proc_id, pi, num_iterations, num_threads);
        break;

    case 1:
        num_iterations = precision / 3;
        check_errors_gmp(num_procs, precision, num_iterations, num_threads, proc_id, algorithm);
        algorithm_type = "Bellard (Processes distributes the iterations in blocks and threads do it cyclically)";
        bellard_blocks_and_cyclic_algorithm_gmp(num_procs, proc_id, pi, num_iterations, num_threads);
        break;

    case 2:
        num_iterations = (precision + 14 - 1) / 14;  //Division por exceso
        check_errors_gmp(num_procs, precision, num_iterations, num_threads, proc_id, algorithm);
        algorithm_type = "Chudnovsky (Processes and threads distributes the iterations in blocks while using the simplified mathematical expression)";
        chudnovsky_blocks_and_blocks_algorithm_gmp(num_procs, proc_id, pi, num_iterations, num_threads);
        break;
    
    case 3:
        num_iterations = (precision + 14 - 1) / 14;  //Division por exceso
        check_errors_gmp(num_procs, precision, num_iterations, num_threads, proc_id, algorithm);
        algorithm_type = "Chudnovsky (Processes distributes the iterations in blocks and threads do it cyclically while using the simplified mathematical expression)";
        chudnovsky_blocks_and_cyclic_algorithm_gmp(num_procs, proc_id, pi, num_iterations, num_threads);
        break;


    default:
        if (proc_id == 0){
            printf("  Algorithm selected is not correct. Try with: \n");
            printf("      algorithm == 0 -> BBP \n");
            printf("      algorithm == 1 -> Bellard \n");
            printf("      algorithm == 2 -> Chudnovsky \n");
            printf("\n");
        } 
        MPI_Finalize();
        exit(-1);
        break;
    }

    //Get time, check decimals, free pi and print the results
    if (proc_id == 0) {  
        gettimeofday(&t2, NULL);
        execution_time = ((t2.tv_sec - t1.tv_sec) * 1000000u +  t2.tv_usec - t1.tv_usec)/1.e6; 
        decimals_computed = check_decimals_gmp(pi);
        if (print_in_csv_format) { print_results_csv("GMP", algorithm_type, precision, num_iterations, num_procs, num_threads, decimals_computed, execution_time); } 
        else { print_results("GMP", algorithm_type, precision, num_iterations, num_procs, num_threads, decimals_computed, execution_time); }
        mpf_clear(pi);
    }

}



