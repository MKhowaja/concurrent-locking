// Murray A. Eisenberg and Michael R. McGuire}, Further Comments on Dijkstra's Concurrent Programming Control Problem,
// CACM, 1972, 15(11), p. 999

enum Intent { DontWantIn, WantIn, EnterCS };
volatile TYPE *control, HIGH;

static void *Worker( void *arg ) {
	unsigned int id = (size_t)arg;
	int j;
#ifdef FAST
	unsigned int cnt = 0;
#endif // FAST
	size_t entries[RUNS];

	for ( int r = 0; r < RUNS; r += 1 ) {
		entries[r] = 0;
		while ( stop == 0 ) {
#ifdef FAST
			id = startpoint( cnt );						// different starting point each experiment
			cnt = cycleUp( cnt, NoStartPoints );
#endif // FAST
		  L0: control[id] = WantIn;						// entry protocol
			Fence();									// force store before more loads
			// step 1, wait for threads with higher priority
		  L1: for ( j = HIGH; j != id; j = cycleUp( j, N ) )
				if ( control[j] != DontWantIn ) { Pause(); goto L1; } // restart search
			control[id] = EnterCS;
			Fence();									// force store before more loads
			// step 2, check for any other thread finished step 1
			for ( j = 0; j < N; j += 1 )
				if ( j != id && control[j] == EnterCS ) goto L0;
			if ( control[HIGH] != DontWantIn && HIGH != id ) goto L0;
			HIGH = id;									// its now ok to enter
			CriticalSection( id );
			// look for any thread that wants in other than this thread
//			for ( j = cycleUp( id + 1, N );; j = cycleUp( j, N ) ) // exit protocol
			for ( j = cycleUp( HIGH + 1, N );; j = cycleUp( j, N ) ) // exit protocol
				if ( control[j] != DontWantIn ) { HIGH = j; break; }
			control[id] = DontWantIn;
			entries[r] += 1;
		} // while
		__sync_fetch_and_add( &Arrived, 1 );
		while ( stop != 0 ) Pause();
		__sync_fetch_and_add( &Arrived, -1 );
	} // for
	qsort( entries, RUNS, sizeof(size_t), compare );
	return (void *)median(entries);
} // Worker

void ctor() {
	control = Allocator( sizeof(volatile TYPE) * N );
	for ( int i = 0; i < N; i += 1 ) {					// initialize shared data
		control[i] = DontWantIn;
	} // for
	HIGH = 0;
} // ctor

void dtor() {
	free( (void *)control );
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DAlgorithm=Eisendberg Harness.c -lpthread -lm" //
// End: //
