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

/*
 * Called by the driver during initialization.
 */

 /*
 haoyu guo: haoyuguo@buffalo.edu
 UBID:50087555
 */
<<<<<<< HEAD
 // static struct lock *intersection_lock;
 // static struct lock *car0_lock;
 // static struct lock *car1_lock;
 // static struct lock *car2_lock;
 // static struct lock *car3_lock;
 //
 // static struct cv *car0_cv;
 // static struct cv *car1_cv;
 // static struct cv *car2_cv;
 // static struct cv *car3_cv;
 //
 // bool intersection[4];
 // static struct cv *cars_cv[4];
 // bool car_path[4][4];
=======
 static struct lock *intersection_lock;
 static struct lock *car0_lock;
 static struct lock *car1_lock;
 static struct lock *car2_lock;
 static struct lock *car3_lock;

 static struct cv *car0_cv;
 static struct cv *car1_cv;
 static struct cv *car2_cv;
 static struct cv *car3_cv;

 bool intersection[4];
 static struct cv *cars_cv[4];
 bool car_path[4][4];
>>>>>>> rwlock_mmz


void
stoplight_init() {
<<<<<<< HEAD
	// intersection_lock = lock_create("intersection_lock");
	// car0_lock = lock_create("car0_lock");
	// car1_lock = lock_create("car1_lock");
	// car2_lock = lock_create("car2_lock");
	// car3_lock = lock_create("car3_lock");
	//
	// cars_cv[0] = cv_create("car0_cv");
	// cars_cv[1] = cv_create("car1_cv");
	// cars_cv[2] = cv_create("car2_cv");
	// cars_cv[3] = cv_create("car3_cv");
	//
	// intersection[0] = false;
	// intersection[1] = false;
	// intersection[2] = false;
	// intersection[3] = false;
	//
	// for(int i = 0; i < 4; ++i){
	// 	for(int j = 0; j < 4; ++j){
	// 		car_path[i][j] = false;
	// 	}
	// }
=======
	intersection_lock = lock_create("intersection_lock");
	car0_lock = lock_create("car0_lock");
	car1_lock = lock_create("car1_lock");
	car2_lock = lock_create("car2_lock");
	car3_lock = lock_create("car3_lock");

	cars_cv[0] = cv_create("car0_cv");
	cars_cv[1] = cv_create("car1_cv");
	cars_cv[2] = cv_create("car2_cv");
	cars_cv[3] = cv_create("car3_cv");

	intersection[0] = false;
	intersection[1] = false;
	intersection[2] = false;
	intersection[3] = false;

	for(int i = 0; i < 4; ++i){
		for(int j = 0; j < 4; ++j){
			car_path[i][j] = false;
		}
	}
>>>>>>> rwlock_mmz

	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
<<<<<<< HEAD
	// lock_destroy(intersection_lock);
	// lock_destroy(car0_lock);
	// lock_destroy(car1_lock);
	// lock_destroy(car2_lock);
	// lock_destroy(car3_lock);
	//
	// cv_destroy(car0_cv);
	// cv_destroy(car1_cv);
	// cv_destroy(car2_cv);
	// cv_destroy(car3_cv);
	//
	// cv_destroy(cars_cv[0]);
	// cv_destroy(cars_cv[1]);
	// cv_destroy(cars_cv[2]);
	// cv_destroy(cars_cv[3]);
=======
	lock_destroy(intersection_lock);
	lock_destroy(car0_lock);
	lock_destroy(car1_lock);
	lock_destroy(car2_lock);
	lock_destroy(car3_lock);

	cv_destroy(car0_cv);
	cv_destroy(car1_cv);
	cv_destroy(car2_cv);
	cv_destroy(car3_cv);

	cv_destroy(cars_cv[0]);
	cv_destroy(cars_cv[1]);
	cv_destroy(cars_cv[2]);
	cv_destroy(cars_cv[3]);
>>>>>>> rwlock_mmz

	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
<<<<<<< HEAD
	(void) direction;
	(void) index;
	// car_path[index][direction] = true;
	//
	// lock_acquire(intersection_lock);
	//
	// if(intersection[direction]){
	// 	cv_wait(cars_cv[index],intersection_lock);
	// }
	//
	// intersection[direction] = true; // intersection occupied
	//
	//
	// inQuadrant(direction, index);
	// leaveIntersection(index);
	// intersection[index] = false;
	//
	// car_path[index][direction] = false;
	// /*helper function*/
	// for(int c = 0; c < 4; ++c){
	// 	bool flag = false;
	// 	for(int i = 0; i < 4; ++i){
	// 		// check direction
	// 		if(car_path[c][i] && (!intersection[i])){
	// 			flag = true;
	// 		}
	// 		if(flag){
	// 			cv_signal(cars_cv[c], intersection_lock);
	// 			break;
	// 		}
	// 	}
	// }
	/*helper function*/

	//lock_release(intersection_lock);
=======
	car_path[index][direction] = true;

	lock_acquire(intersection_lock);

	if(intersection[direction]){
		cv_wait(cars_cv[index],intersection_lock);
	}

	intersection[direction] = true; // intersection occupied


	inQuadrant(direction, index);
	leaveIntersection(index);
	intersection[index] = false;

	car_path[index][direction] = false;
	/*helper function*/
	for(int c = 0; c < 4; ++c){
		bool flag = false;
		for(int i = 0; i < 4; ++i){
			// check direction
			if(car_path[c][i] && (!intersection[i])){
				flag = true;
			}
			if(flag){
				cv_signal(cars_cv[c], intersection_lock);
				break;
			}
		}
	}
	/*helper function*/

	lock_release(intersection_lock);
>>>>>>> rwlock_mmz


	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	(void) direction;
	(void)index;
	// car_path[index][direction] = true;
	//
	// lock_acquire(intersection_lock);
	//
	// if(intersection[direction]){
	// 	cv_wait(cars_cv[index],intersection_lock);
	// }
	//
	// intersection[direction] = true; // intersection occupied
	//
	//
	// inQuadrant(direction, index);
	// leaveIntersection(index);
	// intersection[index] = false;
	//
	// car_path[index][direction] = false;
	// /*helper function*/
	// for(int c = 0; c < 4; ++c){
	// 	bool flag = false;
	// 	for(int i = 0; i < 4; ++i){
	// 		// check direction
	// 		if(car_path[c][i] && (!intersection[i])){
	// 			flag = true;
	// 		}
	// 		if(flag){
	// 			cv_signal(cars_cv[c], intersection_lock);
	// 			break;
	// 		}
	// 	}
	// }
	// /*helper function*/
	//
	// lock_release(intersection_lock);


















	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	(void)direction;
	(void)index;
	/*
	 * Implement this function.
	 */
	return;
}
