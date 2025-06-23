/**
 * File: thread-pool.h
 * -------------------
 * This class defines the ThreadPool class, which accepts a collection
 * of thunks (which are zero-argument functions that don't return a value)
 * and schedules them in a FIFO manner to be executed by a constant number
 * of child threads that exist solely to invoke previously scheduled thunks.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstddef>     // for size_t
#include <functional>  // for the function template used in the schedule signature
#include <thread>      // for thread
#include <vector>      // for vector
#include <mutex>       // for mutex
#include <condition_variable> // for condition_variable
#include <queue>       // for queue
#include "Semaphore.h" // for Semaphore

using namespace std;


/**
 * @brief Represents a worker in the thread pool.
 * 
 * The `worker_t` struct contains information about a worker 
 * thread in the thread pool. Should be includes the thread object, 
 * availability status, the task to be executed, and a semaphore 
 * (or condition variable) to signal when work is ready for the 
 * worker to process.
 */
typedef struct worker {
    thread ts;                        // Thread asociado al worker, se ejecuta con el método worker(...)
    function<void(void)> thunk;       // Función que debe ejecutar este worker cuando es asignado por el dispatcher
    Semaphore ready;                  // Semáforo que el dispatcher usa para avisar al worker que tiene una tarea lista
    bool busy = false;                // Indica si el worker está ocupado ejecutando una tarea (true) o disponible (false)
    mutex lock;                       // Mutex que protege el acceso a 'thunk' y 'busy', para evitar condiciones de carrera
    // constructor explicito
    worker() : ready(0){}

  } worker_t;

class ThreadPool {
  public:

  /**
  * Constructs a ThreadPool configured to spawn up to the specified
  * number of threads.
  */
    ThreadPool(size_t numThreads);

  /**
  * Schedules the provided thunk (which is something that can
  * be invoked as a zero-argument function without a return value)
  * to be executed by one of the ThreadPool's threads as soon as
  * all previously scheduled thunks have been handled.
  */
    void schedule(const function<void(void)>& thunk);

  /**
  * Blocks and waits until all previously scheduled thunks
  * have been executed in full.
  */
    void wait();

  /**
  * Waits for all previously scheduled thunks to execute, and then
  * properly brings down the ThreadPool and any resources tapped
  * over the course of its lifetime.
  */
    ~ThreadPool();
    
  private:

    void worker(int id);
    void dispatcher();
    thread dt;                              // dispatcher thread handle
    vector<worker_t> wts;                   // worker thread handles. you may want to change/remove this
    bool done;                              // flag to indicate the pool is being destroyed
    mutex queueLock;                        // mutex to protect the queue of tasks

    /* It is incomplete, there should be more private variables to manage the structures... 
    * *
    */

    queue<function<void(void)>> taskQueue; // Cola de tareas pendientes, donde se almacenan los thunks a ejecutar

    int tasksInProgress = 0;               // Número de workers que actualmente están ejecutando una tarea
                                           // Se usa para saber si el pool sigue activo

    condition_variable_any allDoneCV;      // Variable condicional utilizada en el método wait()
                                           // Permite al hilo principal bloquearse hasta que no queden tareas pendientes

    Semaphore tasksAvailable;              // Semáforo que cuenta cuántas tareas están disponibles en la cola
                                           // El dispatcher hace wait() sobre este semáforo para dormir hasta que haya trabajo

    /* ThreadPools are the type of thing that shouldn't be cloneable, since it's
    * not clear what it means to clone a ThreadPool (should copies of all outstanding
    * functions to be executed be copied?).
    *
    * In order to prevent cloning, we remove the copy constructor and the
    * assignment operator.  By doing so, the compiler will ensure we never clone
    * a ThreadPool. */
    ThreadPool(const ThreadPool& original) = delete;
    ThreadPool& operator=(const ThreadPool& rhs) = delete;
};
#endif
