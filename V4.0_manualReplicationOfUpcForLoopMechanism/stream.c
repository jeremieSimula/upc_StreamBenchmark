# include <stdio.h>
# include <unistd.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <sys/time.h>

#include <upc.h>
#include <upc_io.h>

//define BLOCKSIZE for distribution of arrays
//THIS HAS TO BE CHANGED TO numtet/THREADS!!!
//BLOCKSIZE defines, how many elements in a row 
//are assigned to the same THREAD
#ifdef BLOCKSIZE
	
#else
       #define BLOCKSIZE 1
#endif


#ifndef STREAM_ARRAY_SIZE
#   define STREAM_ARRAY_SIZE	10000000

#endif

//#   define STREAM_ARRAY_SIZE_Foreigner STREAM_ARRAY_SIZE-(STREAM_ARRAY_SIZE/THREADS)

#ifdef NTIMES
#if NTIMES<=1
#   define NTIMES	10
#endif
#endif
#ifndef NTIMES
#   define NTIMES	10
#endif

#ifndef OFFSET
#   define OFFSET	0
#endif


# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

shared [BLOCKSIZE] STREAM_TYPE*	a;
shared [BLOCKSIZE] STREAM_TYPE*	b;
shared [BLOCKSIZE] STREAM_TYPE*	c;




static double	avgtime[4] = {0}, maxtime[4] = {0},
		mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};

static char	*label[4] = {"Copy:      ", "Scale:     ",
    "Add:       ", "Triad:     "};

static double	bytes[4] = {
    2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE
    };

extern double mysecond();
extern void checkSTREAMresults();

#ifdef TUNED
extern void tuned_STREAM_Copy();
extern void tuned_STREAM_Scale(STREAM_TYPE scalar);
extern void tuned_STREAM_Add();
extern void tuned_STREAM_Triad(STREAM_TYPE scalar);
#endif


shared int		threadCounter = 0;

int main()
{

    a = upc_all_alloc(THREADS, sizeof(STREAM_TYPE)*STREAM_ARRAY_SIZE/THREADS);
    b = upc_all_alloc(THREADS, sizeof(STREAM_TYPE)*STREAM_ARRAY_SIZE/THREADS);
    c = upc_all_alloc(THREADS, sizeof(STREAM_TYPE)*STREAM_ARRAY_SIZE/THREADS);
    //a = (shared [BLOCKSIZE] STREAM_TYPE*) upc_all_alloc(sizeof(double), STREAM_ARRAY_SIZE+OFFSET);
    //b = (shared [BLOCKSIZE] STREAM_TYPE*) upc_all_alloc(sizeof(double), STREAM_ARRAY_SIZE+OFFSET);
    //c = (shared [BLOCKSIZE] STREAM_TYPE*) upc_all_alloc(sizeof(double), STREAM_ARRAY_SIZE+OFFSET);

    int             quantum, checktick();
    int             BytesPerWord;
    int             k;
    int             foreignCounter;
    int             STREAM_ARRAY_SIZE_FOREIGNER = STREAM_ARRAY_SIZE-(STREAM_ARRAY_SIZE/THREADS);
    ssize_t		    j;
    STREAM_TYPE	    scalar;
    double	        t;
    double          times[4][NTIMES];



    /* --- SETUP --- determine precision and check timing --- */
    if(MYTHREAD == 0)
    {

        int *foreigners;
        foreigners = malloc(STREAM_ARRAY_SIZE*sizeof(int));
        for(k=0;k<STREAM_ARRAY_SIZE;k++)
        {
            foreigners[k]=0;
        }


        upc_barrier;

        printf(HLINE);
        printf("STREAM version $Revision: 5.10 $\n");
        printf(HLINE);
        BytesPerWord = sizeof(STREAM_TYPE);
        printf("This system uses %d bytes per array element.\n", BytesPerWord);

        printf(HLINE);
#ifdef N
        printf("*****  WARNING: ******\n");
        printf("      It appears that you set the preprocessor variable N when compiling this code.\n");
        printf("      This version of the code uses the preprocesor variable STREAM_ARRAY_SIZE to control the array size\n");
        printf("      Reverting to default value of STREAM_ARRAY_SIZE=%llu\n",(unsigned long long) STREAM_ARRAY_SIZE);
        printf("*****  WARNING: ******\n");
#endif

        printf("Array size = %llu (elements), Offset = %d (elements)\n" , (unsigned long long) STREAM_ARRAY_SIZE, OFFSET);
        printf("Memory per array = %.1f MiB (= %.1f GiB).\n", 
	    BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0),
	    BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0/1024.0));

        printf("Total memory required = %.1f MiB (= %.1f GiB).\n",
	    (3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.),
	    (3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024./1024.));

        printf("Each kernel will be executed %d times.\n", NTIMES);
        printf(" The *best* time for each kernel (excluding the first iteration)\n"); 
        printf(" will be used to compute the reported bandwidth.\n");
        printf(HLINE);
        printf ("Number of Threads requested = %i\n",THREADS);
    }



	threadCounter = 0;
    upc_barrier;
	threadCounter++;
    upc_barrier;

    if(MYTHREAD==0)
    {
        printf ("Number of Threads counted = %i\n", threadCounter);
    }

    upc_forall(j=0; j<STREAM_ARRAY_SIZE; j++;&a[j])
    {
        a[j] = 1.0;
        b[j] = 2.0;
        c[j] = 0.0;
    }


    /* Get initial value for system clock. */
    if(MYTHREAD == 0)
    {


        printf(HLINE);

        if  ( (quantum = checktick()) >= 1)
        {
	        printf("Your clock granularity/precision appears to be %d microseconds.\n", quantum);
        }
        else
        {
    	    printf("Your clock granularity appears to be less than one microsecond.\n");
	        quantum = 1;
        }

        t = mysecond();
    }

    upc_barrier;

    upc_forall(j = 0; j < STREAM_ARRAY_SIZE; j++;&a[j])
    {
        a[j] = 2.0E0 * a[j];
    }


    if(MYTHREAD == 0)
    {

        t = 1.0E6 * (mysecond() - t);

        printf("Each test below will take on the order"
	    " of %d microseconds.\n", (int) t  );
        printf("   (= %d clock ticks)\n", (int) (t/quantum) );
        printf("Increase the size of the arrays if this shows that\n");
        printf("you are not getting at least 20 clock ticks per test.\n");

        printf(HLINE);

//        printf("WARNING -- The above is only a rough guideline.\n");
//        printf("For best results, please be sure you know the\n");
//        printf("precision of your system timer.\n");

        printf(HLINE);
        int *remoteCoordinates;
        remoteCoordinates = malloc(STREAM_ARRAY_SIZE_FOREIGNER * sizeof(int));
        for(k=0;k<STREAM_ARRAY_SIZE;k++)
        {
            if((int)upc_threadof(&a[k]) != 0)
            {
                foreignCounter++;
                //foreigners[k]=(int)upc_threadof(&a[k]);
                
            }
            else
            {
                //foreigners[k]=1024;
            }
        }
        printf("Number of foreigners in A: %d \n",foreignCounter);
        printf(HLINE);
    }
    









/*************************************************************/
    /*	--- MAIN LOOP --- repeat test cases NTIMES times --- */
/*************************************************************/


scalar = 3.0;

//blocksize loop counter

int remainder = STREAM_ARRAY_SIZE%(THREADS);
for (k=0; k<NTIMES; k++)
{
    if(MYTHREAD == 0)
    {
        times[0][k] = mysecond();
    }

    int bl = 0;
    for(bl=0; bl<STREAM_ARRAY_SIZE; bl++)
    {
        if(upc_threadof(&c[bl]) == MYTHREAD)
        {
            c[bl] = a[bl];
        }
    }
    

    if(MYTHREAD == 0)
    {
        times[0][k] = mysecond() - times[0][k];
        times[1][k] = mysecond();
    }
    

    for(bl=0; bl<STREAM_ARRAY_SIZE; bl++)
    {
        if(upc_threadof(&c[bl]) == MYTHREAD)
        {
            b[bl] = scalar*c[bl];
        }
    }
    
    if(MYTHREAD == 0)
    {
        times[1][k] = mysecond() - times[1][k];
        times[2][k] = mysecond();
    }

    for(bl=0; bl<STREAM_ARRAY_SIZE; bl++)
    {
        if(upc_threadof(&c[bl]) == MYTHREAD)
        {
            c[bl] = a[bl]+b[bl];
        }
    }
       
 
    if(MYTHREAD == 0)
    {   
        times[2][k] = mysecond() - times[2][k];
        times[3][k] = mysecond();
    }
    for(bl=0; bl<STREAM_ARRAY_SIZE; bl++)
    {
        if(upc_threadof(&c[bl]) == MYTHREAD)
        {
            a[bl] = b[bl]+scalar*c[bl];
        }
    }

    if(MYTHREAD == 0)
    {
        times[3][k] = mysecond() - times[3][k];
    }
}


    /*	--- SUMMARY --- */ 
    if(MYTHREAD==0)
    {
        for (k=1; k<NTIMES; k++) /* note -- skip first iteration */
        {
            for (j=0; j<4; j++)
            {
                avgtime[j] = avgtime[j] + times[j][k];
                mintime[j] = MIN(mintime[j], times[j][k]);
                maxtime[j] = MAX(maxtime[j], times[j][k]);
            }
        }

        printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
    
        for (j=0; j<4; j++)
        {
            avgtime[j] = avgtime[j]/(double)(NTIMES-1);

            printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
            1.0E-06 * bytes[j]/mintime[j],
            avgtime[j],
            mintime[j],
            maxtime[j]);
        }
        printf(HLINE);

        /* --- Check Results --- */
        checkSTREAMresults();
        printf(HLINE);
    }
    upc_barrier;
    return 0;
}





















































# define	M	20

int
checktick()
{
    int		i, minDelta, Delta;
    double	t1, t2, timesfound[M];

/*  Collect a sequence of M unique time values from the system. */

    for (i = 0; i < M; i++)
    {
	    t1 = mysecond();
	    while( ((t2=mysecond()) - t1) < 1.0E-6 );
	    timesfound[i] = t1 = t2;
	}

    /*
     * Determine the minimum difference between these M values.
     * This result will be our estimate (in microseconds) for the
     * clock granularity.
     */

    minDelta = 1000000;
    for (i = 1; i < M; i++)
    {
	    Delta = (int)( 1.0E6 * (timesfound[i]-timesfound[i-1]));
	    minDelta = MIN(minDelta, MAX(Delta,0));
	}

    return(minDelta);
}



/* A gettimeofday routine to give access to the wall
   clock timer on most UNIX-like systems.  */

#include <sys/time.h>

double mysecond()
{
    struct timeval tp;
    struct timezone tzp;
    int i;

    i = gettimeofday(&tp,&tzp);
    return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
void checkSTREAMresults ()
{
	STREAM_TYPE aj,bj,cj,scalar;
	STREAM_TYPE aSumErr,bSumErr,cSumErr;
	STREAM_TYPE aAvgErr,bAvgErr,cAvgErr;
	double epsilon;
	ssize_t	j;
	int	k,ierr,err;

    /* reproduce initialization */
	aj = 1.0;
	bj = 2.0;
	cj = 0.0;
    /* a[] is modified during timing check */
	aj = 2.0E0 * aj;
    /* now execute timing loop */
	scalar = 3.0;
	for (k=0; k<NTIMES; k++)
        {
            cj = aj;
            bj = scalar*cj;
            cj = aj+bj;
            aj = bj+scalar*cj;
        }

    /* accumulate deltas between observed and expected results */
	aSumErr = 0.0;
	bSumErr = 0.0;
	cSumErr = 0.0;
	for (j=0; j<STREAM_ARRAY_SIZE; j++) {
		aSumErr += abs(a[j] - aj);
		bSumErr += abs(b[j] - bj);
		cSumErr += abs(c[j] - cj);
		// if (j == 417) printf("Index 417: c[j]: %f, cj: %f\n",c[j],cj);	// MCCALPIN
	}
	aAvgErr = aSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;
	bAvgErr = bSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;
	cAvgErr = cSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;

	if (sizeof(STREAM_TYPE) == 4) {
		epsilon = 1.e-6;
	}
	else if (sizeof(STREAM_TYPE) == 8) {
		epsilon = 1.e-13;
	}
	else {
		printf("WEIRD: sizeof(STREAM_TYPE) = %lu\n",sizeof(STREAM_TYPE));
		epsilon = 1.e-6;
	}

	err = 0;
	if (abs(aAvgErr/aj) > epsilon) {
		err++;
		printf ("Failed Validation on array a[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",aj,aAvgErr,abs(aAvgErr)/aj);
		ierr = 0;
		for (j=0; j<STREAM_ARRAY_SIZE; j++) {
			if (abs(a[j]/aj-1.0) > epsilon) {
				ierr++;
#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array a: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,aj,a[j],abs((aj-a[j])/aAvgErr));
				}
#endif
			}
		}
		printf("     For array a[], %d errors were found.\n",ierr);
	}
	if (abs(bAvgErr/bj) > epsilon) {
		err++;
		printf ("Failed Validation on array b[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",bj,bAvgErr,abs(bAvgErr)/bj);
		printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
		ierr = 0;
		for (j=0; j<STREAM_ARRAY_SIZE; j++) {
			if (abs(b[j]/bj-1.0) > epsilon) {
				ierr++;
#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array b: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,bj,b[j],abs((bj-b[j])/bAvgErr));
				}
#endif
			}
		}
		printf("     For array b[], %d errors were found.\n",ierr);
	}
	if (abs(cAvgErr/cj) > epsilon) {
		err++;
		printf ("Failed Validation on array c[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
		printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",cj,cAvgErr,abs(cAvgErr)/cj);
		printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
		ierr = 0;
		for (j=0; j<STREAM_ARRAY_SIZE; j++) {
			if (abs(c[j]/cj-1.0) > epsilon) {
				ierr++;
#ifdef VERBOSE
				if (ierr < 10) {
					printf("         array c: index: %ld, expected: %e, observed: %e, relative error: %e\n",
						j,cj,c[j],abs((cj-c[j])/cAvgErr));
				}
#endif
			}
		}
		printf("     For array c[], %d errors were found.\n",ierr);
	}
	if (err == 0) {
		printf ("Solution Validates: avg error less than %e on all three arrays\n",epsilon);
	}
#ifdef VERBOSE
	printf ("Results Validation Verbose Results: \n");
	printf ("    Expected a(1), b(1), c(1): %f %f %f \n",aj,bj,cj);
	printf ("    Observed a(1), b(1), c(1): %f %f %f \n",a[1],b[1],c[1]);
	printf ("    Rel Errors on a, b, c:     %e %e %e \n",abs(aAvgErr/aj),abs(bAvgErr/bj),abs(cAvgErr/cj));
#endif
}
