// Your helper functions need to be here.
#include "helpers.h"



void add_job(job_info *job, pid_t pid, List_t *my_list)
{
    bgentry_t *new_bg = (bgentry_t *)malloc(sizeof(bgentry_t));

    // new_bg->job = (job_info *)malloc(sizeof(job_info));
    new_bg->job = job;
    new_bg->pid = pid; // first child process; should this be pid or getpid()?
    time_t my_time;
    time(&my_time);
    new_bg->seconds = my_time;// the time the command was entered;
    insertRear(my_list, new_bg); // add to the back of the list
}





// char *my_itoa(int value, char* result, int base) {
//     // check that the base if valid
//     if (base < 2 || base > 36) { *result = '\0'; return result; }

//     char* ptr = result, *ptr1 = result, tmp_char;
//     int tmp_value;

//     do {
//         tmp_value = value;
//         value /= base;
//         *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
//     } while ( value );

//     // Apply negative sign
//     if (tmp_value < 0) *ptr++ = '-';
//     *ptr-- = '\0';
//     while(ptr1 < ptr) {
//         tmp_char = *ptr;
//         *ptr--= *ptr1;
//         *ptr1++ = tmp_char;
//     }
//     return result;
// }



bool validate_file_names(char* in_file, char* out_file, char* err_file, char *outerr_file)
{
    // keep track of which files exist and which are NULL
    // 1 = exists, 0 = NULL
    int in = 0;
    int out = 0;
    int err = 0;
    int outerr = 0;

    // check if in_file exists
    // if (in_file != NULL) 
    // {
    //     // if out file exists, check if in_file == out_file
    //     if (out_file != NULL)
    //     {
    //         if (strcmp(in_file, out_file) == 0)
    //         {
    //             return false;
    //         }
    //     }
    // }
 
    if (in_file != NULL)     { in = 1; }    // in file exists
    if (out_file != NULL)    { out = 1; }   // out file exists
    if (err_file != NULL)    { err = 1; }   // err file exists
    if (outerr_file != NULL) { outerr = 1; }   // outerr file exists

    if (1 == in & 1 == out & 1 == err & 1 == outerr)
    {
        if (strcmp(in_file, out_file) == 0 | strcmp(out_file, err_file) == 0 |
            strcmp(err_file, outerr_file) == 0)
        {
            return false;
        }
    }
    if (1 == in & 1 == out & 1 == err)
    {
        if (strcmp(in_file, out_file) == 0 | strcmp(out_file, err_file) == 0)
        {
            return false;
        }
    }
    if (1 == in & 1 == out & 1 == outerr)
    {
        if (strcmp(in_file, out_file) == 0 | strcmp(out_file, outerr_file) == 0)
        {
            return false;
        }
    }
    if (1 == in & 1 == err & 1 == outerr)
    {
        if (strcmp(in_file, err_file) == 0 | strcmp(err_file, outerr_file) == 0)
        {
            return false;
        }
    }
    if (1 == out & 1 == err & 1 == outerr)
    {
        if (strcmp(out_file, err_file) == 0 | strcmp(err_file, outerr_file) == 0)
        {
            return false;
        }
    }
    if (1 == in) // if in file == any other file
    {
        if (1 == out)
        {
            if (strcmp(in_file, out_file) == 0)
            {
                return false;
            }
        }
        if (1 == err)
        {
            if (strcmp(in_file, err_file) == 0)
            {
                return false;
            }
        }
        if (1 == outerr)
        {
            if (strcmp(in_file, outerr_file) == 0)
            {
                return false;
            }
        }
    }
    if (1 == out) // if out file == err or outerr file
    {
        if (1 == err)
        {
            if (strcmp(out_file, err_file) == 0)
            {
                return false;
            }
        }
        if (1 == outerr)
        {
            if (strcmp(out_file, outerr_file) == 0)
            {
                return false;
            }
        }
    }
    if (1 == err)
    {
        if (1 == outerr)
        {
            if (strcmp(err_file, outerr_file) == 0)
            {
                return false;
            }
        }
    }


    return true;
}



int my_pow(int base, int power)
{
    int result = 0;
    int k = 0;
    for (k = 0; k < power; k++)
    {
        result+=base;
    }
    return result;
}