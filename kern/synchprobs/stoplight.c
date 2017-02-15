/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define SIZE 4


volatile bool intersection_occupied[SIZE];	// check from each direction if it is occupied. true is occupied, false otherwise
int intersection_count[SIZE];


static struct lock *main_lock;
static struct lock *each_lock[SIZE];
static struct cv *main_cv;
static struct cv *each_cv[SIZE];


void
stoplight_init() {
	intersection_occupied[0] = false;
	intersection_occupied[1] = false;
	intersection_occupied[2] = false;
	intersection_occupied[3] = false;

	intersection_count[0] = 0;
	intersection_count[1] = 0;
	intersection_count[2] = 0;
	intersection_count[3] = 0;



	main_lock = lock_create("main_lock");
	each_lock[0] = lock_create("0_lock");
	each_lock[1] = lock_create("1_lock");
	each_lock[2] = lock_create("2_lock");
	each_lock[3] = lock_create("3_lock");

	main_cv = cv_create("main_cv");
	each_cv[0] = cv_create("0_cv");
	each_cv[1] = cv_create("1_cv");
	each_cv[2] = cv_create("2_cv");
	each_cv[3] = cv_create("3_cv");


	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(main_lock);
	lock_destroy(each_lock[0]);
	lock_destroy(each_lock[1]);
	lock_destroy(each_lock[2]);
	lock_destroy(each_lock[3]);


	cv_destroy(main_cv);
	cv_destroy(each_cv[0]);
	cv_destroy(each_cv[1]);
	cv_destroy(each_cv[2]);
	cv_destroy(each_cv[3]);

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	KASSERT(main_lock != NULL);


	lock_acquire(main_lock);
	// ex direction = 0; check if its occupied
	// apply lock,to make sure put it to sleep
	while(intersection_occupied[direction]) {
		cv_wait(each_cv[direction], main_lock);
		//cv_wait(main_cv, main_lock);
	}

	// put it to occupied,
	intersection_occupied[direction] = true;

	// move to intersection, and leave the intersection
	inQuadrant(direction, index);
	leaveIntersection(index);

	lock_release(main_lock);

	// after leave, the current direction is available again
	intersection_occupied[direction] = false;


	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	// (void) direction;
	// (void)index;
	KASSERT(main_lock != NULL);

	uint32_t destination = (direction + 3) % 4;

	lock_acquire(main_lock);
	// check both direction and destination,  either one of them is occupied, then put sleep
	//ex: direction = 0, destination = 3;
	while(intersection_occupied[direction] || intersection_occupied[destination]){
		cv_wait(each_cv[direction], main_lock);
		cv_wait(each_cv[destination], main_lock);
	}

	// put it to occupied,
	intersection_occupied[direction] = true;
	intersection_occupied[destination] = true;
	// move to intersection, and leave the intersection
	inQuadrant(direction, index);
	inQuadrant(destination, index);
	leaveIntersection(index);

	lock_release(main_lock);

	// after leave, the current direction is available again
	intersection_occupied[direction] = false;
	intersection_occupied[destination] = false;


	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	KASSERT(main_lock != NULL);

	uint32_t stopBy = ( direction + 3 ) % 4;
	uint32_t destination = ( stopBy + 3 ) % 4;

	lock_acquire(main_lock);
	// check both direction and destination,  either one of them is occupied, then put sleep
	while(intersection_occupied[direction] || intersection_occupied[destination] || intersection_occupied[stopBy]){
		cv_wait(each_cv[direction], main_lock);
		cv_wait(each_cv[stopBy], main_lock);
		cv_wait(each_cv[destination], main_lock);
		//cv_wait(main_cv, main_lock);
	}

	// put it to occupied,
	intersection_occupied[direction] = true;
	intersection_occupied[destination] = true;
	intersection_occupied[stopBy] = true;

	// move to intersection, and leave the intersection
	inQuadrant(direction, index);
	inQuadrant(stopBy, index);
	inQuadrant(destination, index);
	leaveIntersection(index);

	lock_release(main_lock);

	// after leave, the current direction is available again
	intersection_occupied[direction] = false;
	intersection_occupied[destination] = false;
	intersection_occupied[stopBy] = false;

	return;
}
