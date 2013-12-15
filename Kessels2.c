// Joep L. W. Kessels, Arbitration Without Common Modifiable Variables, Acta Informatica, 17(2), 1982, p. 137

static volatile TYPE Q[2], R[2];

#define inv( c ) ((c + 1) % 2)
#define plus( a, b ) ((a + b) % 2)

static void *Worker( void *arg ) {
	const unsigned int id = (size_t)arg;
	size_t entries[RUNS];

	size_t rv = (size_t) &rv ; 

	for ( int r = 0; r < RUNS; r += 1 ) {
		entries[r] = 0;
		while ( stop == 0 ) {
			Q[id] = 1;									// entry protocol
			Fence();									// force store before more loads
#if 1
			R[id] = plus( R[inv(id)], id );
			Fence();									// force store before more loads
			while ( Q[inv(id)] == 1 && R[id] == plus( R[inv(id)], id ) ) Pause();
#else
			R[id] = R[1-id] ^ id ;
			Fence();									// force store before more loads
			while ( Q[1-id] == 1 && R[id] == (R[1-id] ^ id) ) Pause() ;
#endif
			CriticalSection( id );
			Q[id] = 0;									// exit protocol
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
	assert( N == 2 );
	Q[0] = Q[1] = 0;
} // ctor

void dtor() {
} // dtor

// Local Variables: //
// tab-width: 4 //
// compile-command: "gcc -Wall -std=gnu99 -O3 -DAlgorithm=Kessels2 Harness.c -lpthread -lm" //
// End: //