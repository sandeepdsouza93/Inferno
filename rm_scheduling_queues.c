/*
 * @rm_scheduling_queues.c
 * @brief Rate-Monotonic Scheduler Task Management
 * @author Sandeep D'souza 
 * 
 * Copyright (c) Carnegie Mellon University, 2017. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "rm_scheduling_queues.h"

// Global Variables for the wait and run queues
struct cpu_run_queue *run_queue;
struct wait_queue wait_q;

/********************** RB Tree functions for run queues -> Each core has a separate runqueue ******************************/
// Add task to the runqueue
int runqueue_add(struct rb_root *head, struct task_struct_sim *task) 
{
    struct rb_node **new = &(head->rb_node), *parent = NULL;
    int result;
    while(*new) {
        struct task_struct_sim *this = container_of(*new, struct task_struct_sim, task_node);
        // order wrt priority
        if(task->T < this->T)
        {
        	result = -1;
        }
        else
        {
        	result = 0;
        }
        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&task->task_node, parent, new);
    rb_insert_color(&task->task_node, head);
    return 0;
}

// Delete task from a runqueue
void runqueue_delete(struct rb_root *head, struct task_struct_sim *task) 
{
    rb_erase(&task->task_node, head);
    return;
}

// Clear All Runqueues of all tasks
void clear_all_runqueues(struct cpu_run_queue *run_queue, int number_cores)
{
    int i;
    struct rb_node *task_node = NULL;
    struct task_struct_sim *task;
    for(i=0; i<number_cores; i++)
    {
        task_node = rb_first(&run_queue[i].head);
        while(task_node!=NULL)
        {
            task = container_of(task_node, struct task_struct_sim, task_node);
            task_node = rb_next(task_node);
            runqueue_delete(&run_queue[i].head ,task);
        }
    } 
}

/********************** RB Tree functions for wait queue -> Single Wait Queue for Scheduler ******************************/
// Add task to the waitqueue
int waitqueue_add(struct rb_root *head, struct task_struct_sim *task) {
    struct rb_node **new = &(head->rb_node), *parent = NULL;
    int result;
    while(*new) {
        struct task_struct_sim *this = container_of(*new, struct task_struct_sim, task_node);
        // order wrt priority
        if(task->arrival_time < this->arrival_time)
        {
        	result = -1;
        }
        else
        {
        	result = 0;
        }
        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&task->task_node, parent, new);
    rb_insert_color(&task->task_node, head);
    return 0;
}

// Delete a task from the wait queue
void waitqueue_delete(struct rb_root *head, struct task_struct_sim *task) 
{
    rb_erase(&task->task_node, head);
    return;
}

// Clear the waitqueue of all the tasks
void clear_waitqueue(struct wait_queue *wait_q)
{
    struct task_struct_sim *task;
    struct rb_node *task_node = NULL;
    struct rb_node *temp_node = NULL;
    task_node = rb_first(&wait_q->head);
    while(task_node != NULL)
    {
        task = container_of(task_node, struct task_struct_sim, task_node);
        temp_node = rb_next(task_node);
        waitqueue_delete(&wait_q->head, task);
        wait_q->task_count--;
        task_node = temp_node;
    }
    return;
}

/************************* Transfer Functions Run Qs <==> Wait Qs ******************************************/
// Move all tasks from the CPU run queues back to wait queue
void move_run_to_wait(struct wait_queue *wait_q, struct cpu_run_queue *run_queue, int no_cores)
{
    struct task_struct_sim *task;
    struct rb_node *task_node = NULL;
    struct rb_node *temp_node = NULL;
    int i;
    int count = 0;

    // Move the tasks on the waitqueues back to the runqueues (to enable a full reset)
    task_node = rb_first(&wait_q->head);
    while(task_node!=NULL)
    {
        task = container_of(task_node, struct task_struct_sim, task_node);
        temp_node = rb_next(task_node);
        waitqueue_delete(&wait_q->head, task);
        runqueue_add(&run_queue[task->cpuid].head, task);
        task_node = temp_node;
    }
    
    // Reassign arrival times -> Move tasks from runqueues to waitqueue
    for(i=0; i<no_cores; i++)
    {
        task_node = rb_first(&run_queue[i].head);
        while(task_node!=NULL)
        {
            count++;
            task = container_of(task_node, struct task_struct_sim, task_node);
            temp_node = rb_next(task_node);
            runqueue_delete(&run_queue[i].head, task);
            run_queue[i].task_count--;
            task->time_executed = 0;
            task->arrival_time = task->arrival_time % task->T;
            waitqueue_add(&wait_q->head, task);
            wait_q->task_count++;
            task_node = temp_node;
        }
    }
    return;
}

// Move tasks that are ready to execute from the waitqueue to its respective runqueue
void move_ready_to_runqueue(struct wait_queue *wait_q, struct cpu_run_queue *run_queue, int sim_count)
{
    struct rb_node *task_node = NULL;
    struct rb_node *temp_node = NULL;
    struct task_struct_sim *task;

    // Move ready tasks to the requested runqueues
    if(wait_q->task_count > 0)
    {
        task_node = rb_first(&wait_q->head);
        while(task_node != NULL)
        {
            task = container_of(task_node, struct task_struct_sim, task_node);
            if(task->arrival_time <= sim_count)
            {
                temp_node = rb_next(task_node);
                waitqueue_delete(&wait_q->head, task);
                wait_q->task_count--;
                runqueue_add(&run_queue[task->cpuid].head, task);
                run_queue[task->cpuid].task_count++;
                task_node = temp_node;
            }
            else
            {
                break;
            }
        }   
    }
    return;
}