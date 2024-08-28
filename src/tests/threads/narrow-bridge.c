
/* File for 'narrow_bridge' task implementation.  
   SPbSTU, IBKS, 2017 */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "narrow-bridge.h"

#define ARR_AT(arr, i, j) arr[i*2+j]

int vehical_count[4] = {0};
int on_bridge; // счетчик машин на мосту
bool start_flag; // флаг начала движения
struct semaphore vehical_sema[4]; // массив семафоров 
enum car_direction curr_dir; // текущее направление
enum car_priority curr_prior; 

static enum car_direction get_new_dir() {
  if(ARR_AT(vehical_count, curr_dir, car_emergency) != 0 && 
      ARR_AT(vehical_count, curr_dir, car_emergency) > ARR_AT(vehical_count, !curr_dir, car_emergency)) 
        return curr_dir;    if(ARR_AT(vehical_count, !curr_dir, car_emergency) != 0) return !curr_dir;
    
  if(ARR_AT(vehical_count, curr_dir, car_normal) > ARR_AT(vehical_count, !curr_dir, car_normal)) 
    return curr_dir;    

  else return !curr_dir;
}

void handle_ready_vehicals() {
  if(on_bridge == 0) {        
    if(ARR_AT(vehical_count, curr_dir, car_emergency) > 1) {
      ARR_AT(vehical_count, curr_dir, car_emergency) -= 2;            
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_emergency));
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_emergency));        
    } else if(ARR_AT(vehical_count, curr_dir, car_emergency) == 1 && ARR_AT(vehical_count, curr_dir, car_normal) != 0) {
      ARR_AT(vehical_count, curr_dir, car_emergency)--;            
      ARR_AT(vehical_count, curr_dir, car_normal)--;
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_emergency));            
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_normal));
    } else if(ARR_AT(vehical_count, curr_dir, car_normal) > 1) {            
      ARR_AT(vehical_count, curr_dir, car_normal) -= 2;
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_normal));            
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_normal));
    } else if(ARR_AT(vehical_count, curr_dir, car_emergency) == 1) {            
      ARR_AT(vehical_count, curr_dir, car_emergency)--;
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_emergency));
    } else if(ARR_AT(vehical_count, curr_dir, car_normal) == 1) {            
      ARR_AT(vehical_count, curr_dir, car_normal)--;
      sema_up(&ARR_AT(vehical_sema, curr_dir, car_normal));        
    }
  }
}

// Called before test. Can initialize some synchronization objects.
void narrow_bridge_init(void) {
  on_bridge = 0;    
  start_flag = false;
  sema_init(&ARR_AT(vehical_sema, dir_left, car_normal), 0);   
  sema_init(&ARR_AT(vehical_sema, dir_left, car_emergency), 0);
  sema_init(&ARR_AT(vehical_sema, dir_right, car_normal), 0);    
  sema_init(&ARR_AT(vehical_sema, dir_right, car_emergency), 0);
}

void arrive_bridge(enum car_priority prio UNUSED, enum car_direction dir UNUSED) {
  if(start_flag == false) {        
    curr_dir = dir;
    start_flag = true;    
  }

  if(on_bridge >= 2) {
    ARR_AT(vehical_count, dir, prio)++;        
    sema_down(&ARR_AT(vehical_sema, dir, prio));
  }

  on_bridge++;
}

void exit_bridge(enum car_priority prio UNUSED, enum car_direction dir UNUSED) {
  on_bridge--;
  curr_dir = get_new_dir();    handle_ready_vehicals();
}


