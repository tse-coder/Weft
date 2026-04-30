# Weft

Work-stealing thread pool & scheduler (C++).

A `submit(task)` thread pool where idle threads steal work from busy
threads' queues instead of blocking.

Progression: basic fixed thread pool with per-thread queues → work
stealing → benchmarking (tasks/sec, p50/p99 latency, CPU utilization,
scaling from 1 to 32 threads).

Core topics: mutexes, condition variables, atomics, lock-free queues,
contention, false sharing, CPU affinity, task scheduling.
