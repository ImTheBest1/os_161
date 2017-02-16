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

static struct lock *main_lock;
static struct lock *each_lock[SIZE];
static struct semaphore *main_sem;

void
stoplight_init() {
	main_lock = lock_create("main_lock");
	each_lock[0] = lock_create("0_lock");
	each_lock[1] = lock_create("1_lock");
	each_lock[2] = lock_create("2_lock");
	each_lock[3] = lock_create("3_lock");
	// prossible situation cause deadlike if there are three cars in one spot.
	//ex: 2 leftturn at location0, 0 start to go into location0, and 1 go straight to location0
	main_sem = sem_create("main_sem", 3);
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

	sem_destroy(main_sem);

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	KASSERT(main_lock != NULL);
	KASSERT(each_lock[0] != NULL);
	KASSERT(each_lock[1] != NULL);
	KASSERT(each_lock[2] != NULL);
	KASSERT(each_lock[3] != NULL);


	P(main_sem); // decrease by 1, one spot will be taken

	lock_acquire(each_lock[direction]);
	// car in direction
	inQuadrant(direction, index);
	// leave
	leaveIntersection(index);
	lock_release(each_lock[direction]);

	V(main_sem); // the spot is available again, increase by 1


	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	// (void) direction;
	// (void)index;
	KASSERT(main_lock != NULL);
	KASSERT(each_lock[0] != NULL);
	KASSERT(each_lock[1] != NULL);
	KASSERT(each_lock[2] != NULL);
	KASSERT(each_lock[3] != NULL);
	KASSERT(main_sem != 0);


	uint32_t destination = (direction + 3) % 4;

	// lock_acquire(main_lock);
	P(main_sem); // decrease by 1, one spot will be taken

	lock_acquire(each_lock[direction]);
	// car in direction
	inQuadrant(direction, index);

	// P(main_sem);
	// lock acquire for destination
	lock_acquire(each_lock[destination]);
	// as long as inQuadrant(destination,index), then can released each_lock[direction], make it available
	inQuadrant(destination, index);
	lock_release(each_lock[direction]);
	// V(main_sem); // the spot is available again, increase by 1


	// inQuadrant(destination, index);
	// leave
	leaveIntersection(index);
	lock_release(each_lock[destination]);

	V(main_sem); // the spot is available again, increase by 1
	// lock_release(main_lock);



	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	KASSERT(main_lock != NULL);
	KASSERT(each_lock[0] != NULL);
	KASSERT(each_lock[1] != NULL);
	KASSERT(each_lock[2] != NULL);
	KASSERT(each_lock[3] != NULL);
	KASSERT(main_sem != NULL);

	uint32_t stopBy = (direction + 3) % 4;
	uint32_t destination = (stopBy + 3) % 4;

	P(main_sem); // decrease by 1, one spot will be taken
	lock_acquire(each_lock[direction]);
	inQuadrant(direction, index);
	//lock_release(each_lock[direction]);

	// P(main_sem);
	lock_acquire(each_lock[stopBy]);
	inQuadrant(stopBy, index);
	lock_release(each_lock[direction]);
	// V(main_sem);
	// inQuadrant(stopBy, index);
	// lock_release(each_lock[direction]);


	lock_acquire(each_lock[destination]);
	// P(main_sem);
	inQuadrant(destination, index);
	// V(main_sem);
	lock_release(each_lock[stopBy]);
	// inQuadrant(destination, index);

	// lock_release(each_lock[stopBy]);
	// leave
	leaveIntersection(index);

	lock_release(each_lock[destination]);
	V(main_sem); // the spot is available again, increase by 1




	return;
}
