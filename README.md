# 📚 Whispering Pages

### A Multithreaded Bookshop Resource Management Simulation in C

**Whispering Pages** simulates a busy bookshop café where multiple book clubs meet simultaneously. Each book club runs as a separate thread and competes for shared resources such as **rooms, chairs, and books**.

The project demonstrates how **multithreading and synchronization** can be used to safely manage concurrent activities and prevent resource conflicts.

## ✨ Key Features

* 🧵 **POSIX Threads** — each book group operates as an independent thread
* 🔒 **Mutex Locks** — protect shared resources from simultaneous access
* 🚦 **POSIX Semaphores** — support controlled resource allocation
* ⏳ **Condition Variables** — make groups wait when enough chairs are unavailable
* 🚪 **Room Scheduling** — prevents overlapping room bookings
* 📖 **Shared Book Catalog** — safely handles book borrowing and returning
* 🪑 **Fair Chair Allocation** — threads wait until sufficient chairs become available
* ♻️ **Automatic Resource Release** — rooms, chairs, and books are returned after meetings
* 🧹 **Proper Cleanup** — synchronization resources are destroyed after execution

## ⚙️ How It Works

**Create Groups → Create Threads → Book Room → Allocate Chairs → Borrow Book → Simulate Meeting → Release Resources**

Each group thread attempts to acquire its required resources. If a resource is unavailable, the thread waits rather than creating a conflict. Once the meeting ends, all resources are released for other groups.

## 🧠 Core Concepts

**Multithreading • Mutual Exclusion • Synchronization • Semaphores • Mutexes • Condition Variables • Resource Allocation • Thread Communication**

## 🎯 Objective

The goal is to demonstrate a practical **concurrent resource-management system in C**, showing how synchronization mechanisms maintain consistency, avoid conflicts, and coordinate multiple threads operating at the same time.

### 🛠️ Technology

**C • POSIX Threads (pthreads) • Mutex • Semaphore • Condition Variable**
