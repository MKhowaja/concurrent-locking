#include "relacy/relacy_std.hpp"
#include "Common.h"

enum { N = 8 };

//======================================================

typedef TYPE QElem_t;

typedef struct CALIGN {
	QElem_t elements[N];
	unsigned int front, rear;
} Queue;

static inline bool QnotEmpty( volatile Queue *queue ) {
	return queue->front != queue->rear;
} // QnotEmpty

static inline void Qenqueue( volatile Queue *queue, QElem_t element ) {
	queue->elements[queue->rear] = element;
	queue->rear = cycleUp( queue->rear, N );
} // Qenqueue

static inline QElem_t Qdequeue( volatile Queue *queue ) {
	QElem_t element = queue->elements[queue->front];
	queue->front = cycleUp( queue->front, N );
	return element;
} // Qdequeue

static inline void Qctor( Queue *queue ) {
	queue->front = queue->rear = 0;
} // Qctor

//======================================================

struct ElevatorQueue : rl::test_suite<ElevatorQueue, N> {
	Queue queue CALIGN;

	typedef struct CALIGN {								// performance gain when fields juxtaposed
		std::atomic<TYPE> apply;
#ifdef FLAG
		std::atomic<TYPE> flag;
#endif // FLAG
	} Tstate;

	std::atomic<TYPE> fast;
	Tstate tstate[N + 1];
	//static volatile TYPE *apply CALIGN;
	std::atomic<TYPE> val[2 * N];

#ifndef CAS
	std::atomic<TYPE> b[N], x, y;
#endif // ! CAS

#ifdef FLAG
	//static volatile TYPE *fast CALIGN;
#else
	std::atomic<TYPE> lock;
#endif // FLAG

#   define await( E ) while ( ! (E) ) Pause()

#ifdef CAS

	bool WCas( TYPE ) { TYPE comp = false; return fast.compare_exchange_strong( comp, true, std::memory_order_seq_cst ); }

#else // ! CAS

#if defined( WCas1 )

	bool WCas( TYPE id ) {
		b[id]($) = true;
		x($) = id;
		if ( y($) != N ) {
			b[id]($) = false;
			return false;
		} // if
		y($) = id;
		if ( x($) != id ) {
			b[id]($) = false;
			for ( int j = 0; j < N; j += 1 )
				await( ! b[j]($) );
			if ( y($) != id ) return false;
		} // if
		bool leader = ((! fast($)) ? fast($) = true : false);
		y($) = N;
		b[id]($) = false;
		return leader;
	} // WCas

#elif defined( WCas2 )

	bool WCas( TYPE id ) {
		b[id]($) = true;
		for ( unsigned int kk = 0; kk < id; kk += 1 ) {
			if ( b[kk]($) ) {
				b[id]($) = false;
				return false ;
			} // if
		} // for
		for ( unsigned int kk = id + 1; kk < N; kk += 1 ) {
			await( ! b[kk]($) );
		} // for
		bool leader = ((! fast($)) ? fast($) = true : false);
		b[id]($) = false;
		return leader;
	} // WCas

#else
    #error unsupported architecture
#endif // WCas

#endif // CAS

	//======================================================

	void before() {
		Qctor( &queue );
		for ( TYPE id = 0; id <= N; id += 1 ) {				// initialize shared data
			tstate[id].apply($) = false;
#ifdef FLAG
			tstate[id].flag($) = false;
#endif // FLAG
		} // for

#ifdef FLAG
		tstate[N].flag($) = true;
#else
		lock($) = N;
#endif // FLAG

		for ( TYPE id = 0; id < N; id += 1 ) {				// initialize shared data
			val[id]($) = N;
			val[N + id]($) = id;
		} // for

#ifdef CAS
		fast($) = false;
#else
		for ( TYPE id = 0; id < N; id += 1 ) {				// initialize shared data
			b[id]($) = false;
		} // for
		y($) = N;
		fast($) = false;
#endif // CAS
	} // before

	rl::var<int> data;

	void thread( TYPE id ) {
		const unsigned int n = N + id, dep = Log2( n );
		typeof(tstate[0].apply) *applyId = &tstate[id].apply;
#ifdef FLAG
		typeof(tstate[0].flag) *flagId = &tstate[id].flag;
		typeof(tstate[0].flag) *flagN = &tstate[N].flag;
#endif // FLAG

#ifdef FAST
		unsigned int cnt = 0, oid = id;
#endif // FAST

		(*applyId)($) = true;
		// loop goes from parent of leaf to child of root
		for ( unsigned int j = (n >> 1); j > 1; j >>= 1 )
			val[j]($) = id;

			if ( WCas( id ) ) {
#ifdef FLAG
				await( (*flagN)($) || (*flagId)($) );
				(*flagN)($) = false;
#else
				await( lock($) == N || lock($) == id );
				lock($) = id;
#endif // FLAG
				fast($) = false;
			} else {
#ifdef FLAG
				await( (*flagId)($) );
#else
				await( lock($) == id );
#endif // FLAG
			} // if
#ifdef FLAG
			(*flagId)($) = false;
#endif // FLAG
			(*applyId)($) = false;

			data($) = id + 1;							// critical section

			// loop goes from child of root to leaf and inspects siblings
			for ( int j = dep - 1; j >= 0; j -= 1 ) { // must be "signed"
				TYPE k = val[(n >> j) ^ 1]($);
				if ( tstate[k].apply($) ) {
					tstate[k].apply($) = false;
					Qenqueue( &queue, k );
				} // if
			}  // for
			if ( QnotEmpty( &queue ) ) {
#ifdef FLAG
				tstate[Qdequeue( &queue )].flag($) = true;
#else
				lock($) = Qdequeue( &queue );
#endif // FLAG
			} else
#ifdef FLAG
				(*flagN)($) = true;
#else
				lock($) = N;
#endif // FLAG
	} // thread
}; // ElevatorQueue

int main() {
	rl::test_params p;
	SetParms( p );
	rl::simulate<ElevatorQueue>( p );
} // main

// Local Variables: //
// tab-width: 4 //
// compile-command: "g++ -Wall -O3 -DNDEBUG -I/u/pabuhr/software/relacy_2_4 ElevatorQueue.cc" //
// End: //
