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
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
 static struct cv *male_cv;
 static struct cv *female_cv;
 static struct cv *matcher_cv;
 static struct lock *whale_lock;

 int male_count;
 int female_count;
 int matcher_count;


void whalemating_init() {
	male_cv = cv_create("male_cv");
	female_cv = cv_create("female_cv");
	matcher_cv = cv_create("matcher_cv");
	whale_lock=lock_create("whale_lock");
	male_count = 0;
	female_count = 0;
	matcher_count = 0;

	return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	lock_destroy(whale_lock);
	cv_destroy(male_cv);
	cv_destroy(female_cv);
	cv_destroy(matcher_cv);
	return;
}

void
male(uint32_t index)
{
	male_start(index);
	lock_acquire(whale_lock);

	male_count++;	// count how many males
	if(female_count != 0 && matcher_count != 0){
		male_count--;

		cv_signal(female_cv, whale_lock);
		cv_signal(matcher_cv, whale_lock);

		female_count--;
		matcher_count--;

	}
	// else if(female_count == 0 && matcher_count != 0){
	// 	cv_wait(male_cv, whale_lock);
	// 	cv_wait(matcher_cv, whale_lock);
	// }else if(female_count != 0 && matcher_count == 0){
	// 	cv_wait(male_cv, whale_lock);
	// 	cv_wait(female_cv, whale_lock);
	// }
	else{
		cv_wait(male_cv, whale_lock);
	}


	lock_release(whale_lock);
	male_end(index);

	return;
}

void
female(uint32_t index)
{
	female_start(index);
	lock_acquire(whale_lock);

	female_count++;	// count how many males
	if(male_count != 0 && matcher_count != 0){
		// perfect matching
		female_count--;
		cv_signal(male_cv, whale_lock); 	//wake up one female
		cv_signal(matcher_cv, whale_lock);	//wake up one matcher

		male_count--;
		matcher_count--;
	}
	// else if(male_count == 0 && matcher_count != 0){ // male isnt coming yet
	// 	cv_wait(female_cv, whale_lock); // go to sleep
	// 	cv_wait(matcher_cv, whale_lock);
	// }else if(male_count != 0 && matcher_count == 0){ // matcher isnt coming yet
	// 	cv_wait(male_cv, whale_lock);
	// 	cv_wait(female_cv, whale_lock);
	// }
	else{
		cv_wait(female_cv, whale_lock);
	}


	lock_release(whale_lock);
	female_end(index);

	return;
}

void
matchmaker(uint32_t index)
{
	matchmaker_start(index);
	lock_acquire(whale_lock);

	matcher_count++;	// count how many males
	if(male_count != 0 && female_count != 0){
		// perfect matching
		matcher_count--;
		cv_signal(female_cv, whale_lock); 	//wake up one female
		cv_signal(male_cv, whale_lock);	//wake up one male

		male_count--;
		female_count--;
	}
	// else if(male_count == 0 && female_count != 0){ // male isnt coming yet
	// 	cv_wait(female_cv, whale_lock); // go to sleep
	// 	cv_wait(matcher_cv, whale_lock);
	// }else if(male_count != 0 && female_count == 0){ // female isnt coming yet
	// 	cv_wait(male_cv, whale_lock);
	// 	cv_wait(matcher_cv, whale_lock);
	// }
	else{
		cv_wait(matcher_cv, whale_lock);
	}

	//matchmaker_end(index);
	lock_release(whale_lock);
	matchmaker_end(index);
	return;
}
