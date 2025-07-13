// Équivalent de mythread.h - abstraction threading

use std::thread::{self, JoinHandle};
use std::sync::{Mutex, Condvar, Once};
use std::time::{Duration, Instant};

// Type pour les threads
pub type MyThread = JoinHandle<()>;

// Type pour les mutex
pub type MyThreadMutex = Mutex<()>;

// Type pour les variables de condition
pub struct MyThreadCond {
    condvar: Condvar,
}

// Type pour les timeouts
pub type MyThreadCondTime = Instant;

// Macro pour synchronisation avec mutex (équivalent de mythread_sync)
macro_rules! mythread_sync {
    ($mutex:expr, $block:block) => {
        {
            let _guard = $mutex.lock().unwrap();
            $block
        }
    };
}

pub(crate) use mythread_sync;

// Fonction once (équivalent de mythread_once)
pub fn mythread_once<F>(f: F) 
where
    F: FnOnce(),
{
    static ONCE: Once = Once::new();
    ONCE.call_once(f);
}

// Création de thread
pub fn mythread_create<F>(f: F) -> Result<MyThread, std::io::Error>
where
    F: FnOnce() + Send + 'static,
{
    thread::Builder::new()
        .spawn(f)
        .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "Failed to create thread"))
}

// Attendre un thread
pub fn mythread_join(thread: MyThread) -> Result<(), std::io::Error> {
    thread.join()
        .map_err(|_| std::io::Error::new(std::io::ErrorKind::Other, "Failed to join thread"))
}

// Initialisation de mutex
pub fn mythread_mutex_init() -> MyThreadMutex {
    Mutex::new(())
}

// Destruction de mutex (no-op en Rust)
pub fn mythread_mutex_destroy(_mutex: MyThreadMutex) {
    // No-op - le Drop trait s'en charge
}

// Lock/unlock de mutex
pub fn mythread_mutex_lock(mutex: &MyThreadMutex) {
    let _ = mutex.lock().unwrap();
}

// Note: En Rust, le unlock se fait automatiquement quand le guard sort de scope

// Initialisation de condition variable
impl MyThreadCond {
    pub fn new() -> Self {
        Self {
            condvar: Condvar::new(),
        }
    }
    
    pub fn signal(&self) {
        self.condvar.notify_one();
    }
    
    pub fn wait(&self, mutex: &MyThreadMutex) {
        let guard = mutex.lock().unwrap();
        let _ = self.condvar.wait(guard).unwrap();
    }
    
    pub fn timedwait(&self, mutex: &MyThreadMutex, timeout: Duration) -> bool {
        let guard = mutex.lock().unwrap();
        let (_, timeout_result) = self.condvar.wait_timeout(guard, timeout).unwrap();
        !timeout_result.timed_out()
    }
}

// Gestion des signaux (stub pour compatibilité)
pub fn mythread_sigmask(_how: i32, _set: Option<&[i32]>, _oset: Option<&mut [i32]>) {
    // Stub - la gestion des signaux est différente en Rust
} 