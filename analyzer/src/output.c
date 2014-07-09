 /**
 *   tcprstat -- Extract stats about TCP response times
 *   Copyright (C) 2010  Ignacio Nin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
**/
 
#include "output.h"
#include "stats.h"
#include "capture.h"
#include "tcprstat.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static int output(time_t current, char format[], unsigned long iterations), output_header(char header[], int verbatim);

// Last output timeval for offline captures
struct timeval last_output;

// Options copy for the same thing
static struct output_options *output_options;

// Iterations
unsigned long iterations;

void *output_thread(void *arg) {
    struct output_options *options; 
    struct timespec ts; // time struct for the timestamp
    time_t current; // current time 
    unsigned long iterations; // the number of output lines
    
    options = arg;
    
    ts = (struct timespec) { options->interval, 0 };  //assign the monitoring interval to the time struct
	ts.tv_sec = 0;
	ts.tv_nsec = 500000; //500 microseconds
	printf("%lld.%.9ld", (long long)ts.tv_sec, ts.tv_nsec);
    
    if (!check_format(options->format))
        abort();
    
    if (options->show_header) {
        if (options->header)
            output_header(options->header, 1);
        else
            output_header(options->format, 0);
        
    }
    
    for (iterations = 0; !options->iterations || iterations < options->iterations;
            iterations ++)
    {
        nanosleep(&ts, NULL);
        
        time(&current);
        output(current, options->format, iterations);
        
    }
    
    // Iterations finished, signal capturing process
    endcapture();
    
    return NULL;
    
}

int output_offline_start(struct output_options *options) {
    // TODO: Make common code with output_offline_start()
    if (!check_format(options->format))
        abort();
    
    if (options->show_header) {
        if (options->header)
            output_header(options->header, 1);
        else
            output_header(options->format, 0);
        
    }
    
    output_options = options;

    iterations = 0;
    
    return 0;
    
}

int output_offline_update(struct timeval tv) {
    struct timeval next;
    
    // Set last output if it's at zero
    if (!last_output.tv_sec)
        last_output = tv;
    
    do {
        next.tv_sec = last_output.tv_sec + output_options->interval;
        next.tv_usec = last_output.tv_usec;
/*        printf("Last output was %lu:%lu, next %lu:%lu. Packet is %lu:%lu\n",
               last_output.tv_sec, last_output.tv_usec, next.tv_sec, next.tv_usec,
               tv.tv_sec, tv.tv_usec);*/
               
    
        if (tv.tv_sec > next.tv_sec ||
                (tv.tv_sec == next.tv_sec && tv.tv_usec > next.tv_usec))
        {
            output(next.tv_sec, output_options->format, iterations);
            last_output = next;
            
            iterations ++;
            
        }
        else
            break;
        
    }
    while (1);
    
    return 0;
        
}

static int output(time_t current, char format[], unsigned long iterations) {
    char *c;
    
    struct stats_results *results;
    
    results = get_flush_stats();
    
    for (c = format; c[0]; c ++)
        if (c[0] == '%') {
            int r = 100;
            c ++;
            
            if (c[0] >= '0' && c[0] <= '9') {
                r = 0;
                while (c[0] >= '0' && c[0] <= '9') {
                    r *= 10;
                    r += c[0] - '0';
                    
                    c ++;
                    
                }
                
            }
            
            if (c[0] == 'n')
                printf("%u", stats_count(results, r));
			/* here is what I have changed */
			else if (c[0] == 'o')
                printf("%u", stats_inr(results, r));
			else if (c[0] == 'u')
                printf("%lu", stats_thrp(results, r));
			/* here is what I have changed */
            else if (c[0] == 'a')
                printf("%lu", stats_avg(results, r));
            else if (c[0] == 's')
                printf("%lu", stats_sum(results, r));
            else if (c[0] == 'x')
                printf("%lu", stats_sqs(results, r));
            else if (c[0] == 'm')
                printf("%lu", stats_min(results, r));
            else if (c[0] == 'M')
                printf("%lu", stats_max(results, r));
            else if (c[0] == 'h')
                printf("%lu", stats_med(results, r));
            else if (c[0] == 'S')
                printf("%lu", stats_std(results, r));
            else if (c[0] == 'v')
                printf("%lu", stats_var(results, r));
            
            // Timestamping
            else if (c[0] == 'I')
                printf("%lu", iterations);
            else if (c[0] == 't' || c[0] == 'T')
                printf("%lu", current - (c[0] == 't' ? timestamp : 0));
            
            // Actual %
            else if (c[0] == '%')
                fputc(c[0], stdout);
            
        }
        else if (c[0] == '\\')
            if (c[1] == 'n') {
                c ++;
                fputc('\n', stdout);
            }
            else if (c[1] == 't') {
                c ++;
                fputc('\t', stdout);
            }
            else if (c[1] == 'r') {
                c ++;
                fputc('\r', stdout);
            }
            else if (c[1] == '\\') {
                c ++;
                fputc('\\', stdout);
            }
            else
                fputc('\\', stdout);
            
        else
            fputc(c[0], stdout);
        
    fflush(stdout);
    
    free_results(results);
    
    return 0;

}

static int output_header(char header[], int verbatim) {
    char *c;
    
    for (c = header; c[0]; c ++)
        if (c[0] == '%') {
            int r = 100;
            c ++;
            
            if (c[0] >= '0' && c[0] <= '9') {
                r = 0;
                while (c[0] >= '0' && c[0] <= '9') {
                    r *= 10;
                    r += c[0] - '0';
                    
                    c ++;
                    
                }
                
            }
            
            if (c[0] == 'n')
                if (r != 100)
                    printf("%d_cnt", r);
                else
                    fputs("count", stdout);
			/* here is what I have changed */
			else if (c[0] == 'o')
                if (r != 100)
                    printf("%d_inr", r);
                else
                    fputs("inr", stdout);
			else if (c[0] == 'u')
                if (r != 100)
                    printf("%d_thrp", r);
                else
                    fputs("thrp", stdout);
			/* here is what I have changed */
            else if (c[0] == 'a')
                if (r != 100)
                    printf("%d_avg", r);
                else
                    fputs("avg", stdout);
            else if (c[0] == 's')
                if (r != 100)
                    printf("%d_sum", r);
                else
                    fputs("sum", stdout);
            else if (c[0] == 'x')
                if (r != 100)
                    printf("%d_sqs", r);
                else
                    fputs("sqs", stdout);
            else if (c[0] == 'm')
                if (r != 100)
                    printf("%d_min", r);
                else
                    fputs("min", stdout);
            else if (c[0] == 'M')
                if (r != 100)
                    printf("%d_max", r);
                else
                    fputs("max", stdout);
            else if (c[0] == 'h')
                if (r != 100)
                    printf("%d_med", r);
                else
                    fputs("med", stdout);
            else if (c[0] == 'S')
                if (r != 100)
                    printf("%d_std", r);
                else
                    fputs("stddev", stdout);
            else if (c[0] == 'v')
                if (r != 100)
                    printf("%d_var", r);
                else
                    fputs("var", stdout);
            
            // Timestamping
            else if (c[0] == 'I')
                fputs("iter#", stdout);
            else if (c[0] == 't')
                fputs("elapsed", stdout);
            else if (c[0] == 'T')
                fputs("timestamp", stdout);
                            
        }
        else if (c[0] == '\\') {
            if (c[1] == 'n') {
                c ++;
                fputc('\n', stdout);
            }
            else if (c[1] == 't') {
                c ++;
                fputc('\t', stdout);
            }
            else if (verbatim) {
                if (c[1] == 'r') {
                    c ++;
                    fputc('\r', stdout);
                }
                else if (c[1] == '\\') {
                    c ++;
                    fputc('\\', stdout);
                }
                else
                    fputc('\\', stdout);
            }
        }
        else if (verbatim)
            fputc(c[0], stdout);
        
            
    fflush(stdout);
    
    return 0;

}

int check_format(char format[]) {
    char *c;
    
    for (c = format; *c; c ++)
        if (c[0] == '%') {
            int r = -1;
            c ++;
            
            switch (c[0]) {
            
            case '0' ... '9':       // GNU extension
                r = 0;
                while (c[0] >= '0' && c[0] <= '9') {
                    r *= 10;
                    r += c[0] - '0';
                    
                    c ++;
                    
                }
                
                if (r <= 0 || r > 100)
                    return 0;
                
                break;
                
            case '%':
                if (r != -1)
                    return 0;
                
            case 'n':
			/* here is what I have changed */
			case 'o':
			case 'u':
			/* here is what I have changed */
            case 'a':
            case 's':
            case 'x':
            case 'r':
            case 'I':
            case 't':
            case 'T':
            case 'm':
            case 'M':
            case 'h':
            case 'S':
            case 'v':
                break;

            default:
                return 0;
                
            }
            
        }
        
    return 1;
    
}

