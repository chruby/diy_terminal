// A header file for helpers.c
// Declare any additional functions in this file
#include "icssh.h"
#include "linkedList.h"

// add a job to the list of bg jobs
void add_job(job_info *job, pid_t pid, List_t *my_list);


// deletes pid bg process from my_list
void delete_job(pid_t pid, List_t *my_list);


// Professor Wong Ma's itoa() function
// char *my_itoa(int num, char *buff, int base);


// make sure none of the input/output files are the same
// return true if none of them are the same, false otherwise
bool validate_file_names(char* in_file, char* out_file, char* err_file, char *outerr_file);


// power of integers
int my_pow(int base, int power);