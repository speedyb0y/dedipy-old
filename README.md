# malloc-hack

This project is supposed to work as a skell for other projects using this model:

  A master process
     N subprocesses
 
 - Each process runs on a specific CPU
 - Only one process per CPU
 - Processes may create threads, but will run on the same CPU
 - The scheduling is FIFO
 
- Processes communicate via pipes
- A timer is set up to keep things running, by interrupting blocking system calls
- 
