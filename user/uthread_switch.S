	.text

/* Switch from current_thread to next thread_thread, and make
 * next_thread the current_thread.  Use t0 as a temporary register,
 * which should be caller saved. */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */

	ret    /* return to ra */
