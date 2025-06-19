/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "thread-pool.h"
#include <stdexcept>   // for runtime_error
using namespace std;

/**
 * Constructor de la clase ThreadPool.
 * Inicializa la estructura de datos interna con la cantidad de threads deseada
 * y marca que el thread pool aún no fue destruido (done = false).
 * 
 * Luego, lanza los hilos de los workers y el hilo del dispatcher.
 */
ThreadPool::ThreadPool(size_t numThreads)
    : wts(numThreads),   // crea un vector de `worker_t` con tamaño `numThreads`
      done(false),       // indica que el pool no está siendo destruido
      tasksAvailable(0)        
    {
    // Por cada worker (uno por posición en el vector wts),
    // lanzamos un hilo que ejecuta el método worker(id).
    for (size_t i = 0; i < numThreads; ++i) {
        wts[i].ts = thread([this, i] {
            this->worker(i); // cada hilo ejecuta worker(i)
        });
    }

    // Finalmente, lanzamos un hilo adicional que ejecuta el método dispatcher().
    // Este hilo se encarga de vigilar la cola de tareas y asignarlas a los workers disponibles.
    dt = thread([this] {
        this->dispatcher(); // el hilo dispatcher se mantiene en ejecución mientras el pool esté activo
    });
}

/**
 * Programa una nueva tarea (thunk) para ser ejecutada por el ThreadPool.
 * La tarea se encola de forma segura y se notifica al dispatcher que hay trabajo disponible.
 * 
 */
void ThreadPool::schedule(const function<void(void)>& thunk) {
    // No se permite programar funciones nulas
    if (!thunk) throw invalid_argument("No se puede usar schedule(): la función es nula.");

    // Si el pool ya fue destruido, no se permite programar nuevas tareas
    if (done) throw runtime_error("No se puede usar schedule(): el pool fue destruido.");

    // Bloqueamos el acceso a la cola para agregar la tarea
    {
        lock_guard<mutex> lock(queueLock);
        taskQueue.push(thunk);  // Encolamos la tarea
    }

    // Notificamos al dispatcher que hay una nueva tarea en espera
    // El semáforo actúa como contador de "tareas disponibles"
    tasksAvailable.signal();  // Aumenta el contador y desbloquea al dispatcher si estaba esperando
}

/**
 * Bloquea la ejecución hasta que todas las tareas hayan sido ejecutadas.
 * 
 * El hilo que llama a wait() se bloquea mientras:
 *  - la cola de tareas no esté vacía, o
 *  - haya workers ejecutando tareas.
 */
void ThreadPool::wait() {
    unique_lock<mutex> lk(waitLock);  // Tomamos control exclusivo del estado global

    // Esperamos a que:
    // - la cola de tareas esté vacía
    // - y no haya workers ejecutando (todos estén libres)
    allDoneCV.wait(lk, [&]() {
        return taskQueue.empty() && tasksInProgress == 0;
    });

    // Cuando se cumple la condición, el hilo que llamó a wait() continúa
}

/**
 * Método que ejecuta cada worker del ThreadPool.
 * Este método corre en su propio hilo. Cada worker:
 *  - espera hasta recibir una señal del dispatcher (mediante el semáforo ready),
 *  - ejecuta la tarea que se le asignó en su campo `thunk`,
 *  - marca que ya está libre para recibir otra tarea.
 */
void ThreadPool::worker(int id) {
    while (true) {
        // El worker se queda dormido hasta que el dispatcher lo despierte
        wts[id].ready.wait();

        // Si el pool se está destruyendo, salimos del bucle
        if (done) break;

        // Bloqueamos su mutex para modificar su estado interno de forma segura
        {
            lock_guard<mutex> lg(wts[id].lock);

            // Si tiene una tarea asignada, la ejecuta
            if (wts[id].thunk) {
                wts[id].thunk();  // Ejecuta la función (tarea)
            }

            // Luego se marca como libre
            wts[id].busy = false;
        }

        // Sección crítica para actualizar la cuenta de workers ocupados
        {
            lock_guard<mutex> guard(waitLock);
            tasksInProgress--;

            // Si no hay tareas en la cola Y no hay ningún worker trabajando,
            // se notifica a quienes estén esperando en wait()
            if (taskQueue.empty() && tasksInProgress == 0) {
                allDoneCV.notify_all();
            }
        }
    }
}
/**
 * Método que ejecuta el dispatcher del ThreadPool.
 * Este hilo especial se queda esperando tareas en la cola
 * y, cuando encuentra una, se la asigna a un worker disponible.
 * 
 * En caso de que no haya ningún worker libre al momento de obtener una tarea,
 * se reintenta más adelante sin extraer la tarea de la cola, evitando perder trabajo
 * y respetando el orden de llegada de las tareas.
 * 
 */
void ThreadPool::dispatcher() {
    while (true) {
        // Esperamos a que haya al menos una tarea disponible en la cola.
        // Este semáforo evita el uso de polling y bloquea al dispatcher eficientemente.
        tasksAvailable.wait();

        // Verificamos si estamos destruyendo el pool
        if (done) break;

        // Mientras haya tareas en la cola y workers disponibles, seguimos despachando
        while (true) {
            // Verificamos si hay tareas en la cola
            {
                unique_lock<mutex> lock(queueLock);
                if (taskQueue.empty()) break;
            }

            size_t available_worker = wts.size();  // índice inválido (fuera de rango válido)

            // Buscamos un worker disponible
            for (size_t i = 0; i < wts.size(); ++i) {
                unique_lock<mutex> lock(wts[i].lock);
                if (!wts[i].busy) {
                    available_worker = i;
                    break;
                }
            }

            // Si no hay ningún worker disponible, esperamos y reintentamos más tarde
            if (available_worker == wts.size()) {
                // Esperamos brevemente para evitar ciclo continuo 
                this_thread::sleep_for(chrono::milliseconds(1));

                // Volvemos a señalar que hay tareas disponibles
                tasksAvailable.signal();  // reactivamos el dispatcher más adelante
                break;
            }

            function<void(void)> task;

            // Obtenemos una tarea de la cola (acceso protegido por mutex)
            {
                unique_lock<mutex> qlock(queueLock);
                if (taskQueue.empty()) break;

                task = taskQueue.front();
                taskQueue.pop();
            }

            // Asignamos la tarea al worker disponible
            {
                unique_lock<mutex> lock(wts[available_worker].lock);

                // Asignamos la tarea y marcamos al worker como ocupado
                wts[available_worker].thunk = task;
                wts[available_worker].busy = true;
            }

            // Indicamos que hay un worker más trabajando
            {
                lock_guard<mutex> guard(waitLock);
                tasksInProgress++;
            }

            // Despertamos al worker para que ejecute su tarea
            wts[available_worker].ready.signal();
        }
    }
}

/**
 * Destructor del ThreadPool.
 * Espera a que todas las tareas hayan sido completadas,
 * luego señala a todos los hilos que deben finalizar,
 * los despierta en caso de que estén bloqueados, y los espera (join).
 */
ThreadPool::~ThreadPool() {
    // Esperamos a que todas las tareas hayan sido completadas
    wait();

    // Indicamos que el pool está siendo destruido
    done = true;

    // Desbloqueamos al dispatcher en caso de que esté esperando en tasksAvailable.wait()
    tasksAvailable.signal();

    // Desbloqueamos a todos los workers en caso de que estén esperando en ready.wait()
    for (auto& w : wts) {
        w.ready.signal();
    }

    // Esperamos (join) a que termine el hilo del dispatcher
    if (dt.joinable()) {
        dt.join();
    }

    // Esperamos (join) a que termine cada hilo de worker
    for (auto& w : wts) {
        if (w.ts.joinable()) {
            w.ts.join();
        }
    }

    // En este punto, todos los hilos terminaron y se cerraron correctamente
}